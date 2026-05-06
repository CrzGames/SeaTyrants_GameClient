#if GAME_ENV_DEV

#include "game/scenes/scene-editormap-cannonsalvo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <numeric>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#include <RC2D/RC2D_filedialog.h>
#include <RC2D/RC2D_keyboard.h>

#include "core/context.h"
#include "game/assets/title-asset-cache.h"
#include "game/camera/camera.h"
#include "game/combat/maritime-cannon-salvo.h"
#include "game/render/world-render-clip.h"
#include "game/scenes/editormap-scene-layout.h"
#include "game/vfx/vfx-classic.h"
#include "game/state.h"

namespace
{
constexpr int kSliderCount = 17;
constexpr int kTrajectorySliderCount = 12;
constexpr int kTrailSliderCount = 21;
constexpr int kManualTrailSliderCount = 5;
constexpr int kLayerButtonCount = 3;
constexpr int kPageButtonCount = 3;
constexpr int kDistanceBandButtonCount = MaritimeCannonSalvoSystem::kProjectileTrajectoryDistanceBandCount;
constexpr int kAngleSectorButtonCount = MaritimeCannonSalvoSystem::kProjectileTrajectoryAngleSectorCount;
constexpr int kShipSpeedActiveSliderIndex = -2;
constexpr int kManualTrailSliderIndexOffset = 100;

constexpr float kPanelWidth = 1360.0f;
constexpr float kPanelHeight = 920.0f;
constexpr float kPanelMargin = 16.0f;
constexpr float kHeaderHeight = 34.0f;
constexpr float kSliderRowHeight = 28.0f;
constexpr float kSliderRowGap = 8.0f;
constexpr float kSliderTrackWidth = 204.0f;
constexpr float kSliderTrackHeight = 8.0f;
constexpr float kSliderKnobWidth = 12.0f;
constexpr float kSliderKnobHeight = 16.0f;
constexpr float kButtonHeight = 28.0f;
constexpr float kScrollBarWidth = 12.0f;
constexpr float kHideButtonWidth = 104.0f;
constexpr float kResetButtonWidth = 152.0f;
constexpr float kLayerButtonWidth = 74.0f;
constexpr float kPageButtonWidth = 132.0f;
constexpr float kDistanceBandButtonWidth = 98.0f;
constexpr float kAngleSectorButtonWidth = 70.0f;
constexpr float kTrajectoryDebugButtonWidth = 58.0f;
constexpr float kTrajectoryDebugButtonGap = 8.0f;
constexpr float kManualStampSelectRadiusPx = 18.0f;
constexpr int kDebugCurveSampleCount = 36;
constexpr float kDebugMarkerSize = 6.0f;
constexpr float kPi = 3.14159265358979323846f;

constexpr RC2D_Color kPanelFill = RC2D_Color{7, 14, 25, 236};
constexpr RC2D_Color kPanelBorder = RC2D_Color{184, 132, 30, 245};
constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 236};
constexpr RC2D_Color kHeaderBadgeFill = RC2D_Color{140, 33, 20, 240};
constexpr RC2D_Color kRowFill = RC2D_Color{10, 16, 27, 214};
constexpr RC2D_Color kTrackFill = RC2D_Color{28, 35, 46, 240};
constexpr RC2D_Color kTrackFillHover = RC2D_Color{44, 54, 68, 244};
constexpr RC2D_Color kTrackFillActive = RC2D_Color{92, 68, 38, 244};
constexpr RC2D_Color kTrackBorder = RC2D_Color{118, 126, 140, 232};
constexpr RC2D_Color kTrackValueFill = RC2D_Color{227, 210, 153, 244};
constexpr RC2D_Color kKnobFill = RC2D_Color{237, 224, 186, 250};
constexpr RC2D_Color kKnobBorder = RC2D_Color{101, 58, 16, 255};
constexpr RC2D_Color kTextPrimary = RC2D_Color{217, 200, 134, 255};
constexpr RC2D_Color kTextMuted = RC2D_Color{176, 184, 194, 255};
constexpr RC2D_Color kTextStrong = RC2D_Color{242, 236, 214, 255};
constexpr RC2D_Color kButtonFill = RC2D_Color{42, 33, 24, 238};
constexpr RC2D_Color kButtonFillHover = RC2D_Color{92, 68, 38, 246};
constexpr RC2D_Color kButtonFillSecondary = RC2D_Color{27, 35, 46, 238};
constexpr RC2D_Color kButtonBorderMuted = RC2D_Color{150, 156, 166, 228};
constexpr RC2D_Color kButtonActiveFill = RC2D_Color{86, 118, 68, 230};
constexpr RC2D_Color kButtonActiveBorder = RC2D_Color{208, 225, 182, 240};
constexpr RC2D_Color kPreviewFill = RC2D_Color{12, 21, 35, 236};
constexpr RC2D_Color kPreviewBorder = RC2D_Color{111, 141, 168, 236};
constexpr RC2D_Color kPreviewGrid = RC2D_Color{44, 58, 74, 170};
constexpr RC2D_Color kHelpFill = RC2D_Color{16, 26, 41, 240};
constexpr RC2D_Color kHelpBorder = RC2D_Color{171, 137, 58, 236};
constexpr RC2D_Color kHelpAccent = RC2D_Color{232, 211, 152, 255};

extern Ship* gActiveCannonSalvoAttackerShip;
extern Ship* gActiveCannonSalvoTargetShip;
extern bool gEditorGlowModeDefault;
extern std::string gEditorGlowLiveSourcePath;
extern bool gEditorGlowLiveClearSourceRequested;
extern std::string gEditorTrailSourcePath;
extern bool gEditorTrailClearSourceRequested;
static bool tryGetTrajectoryPreviewShips(SDL_FPoint* outAttackerTile, SDL_FPoint* outTargetTile);

struct SliderSpec
{
    const char* label;
    float minValue;
    float maxValue;
    float wheelStep;
};

struct HelpEntry
{
    const char* title;
    const char* body;
};

constexpr std::array<SliderSpec, kSliderCount> kSliderSpecs = {{
    {"Intensite glow", 0.0f, 3.0f, 0.05f},
    {"Opacite halo", 0.0f, 1.5f, 0.02f},
    {"Rayon halo", 0.25f, 2.5f, 0.02f},
    {"Dispersion halo", 0.0f, 2.0f, 0.02f},
    {"Rondeur halo", 0.35f, 2.5f, 0.02f},
    {"Core sharpness", 0.35f, 4.0f, 0.03f},
    {"Intensite centre", 0.0f, 2.5f, 0.02f},
    {"Reduction flash centre", 0.0f, 0.95f, 0.01f},
    {"Douceur bords", 0.35f, 2.5f, 0.02f},
    {"Coeur blanc", 0.0f, 1.5f, 0.02f},
    {"Rayon coeur blanc", 0.2f, 1.5f, 0.02f},
    {"Shell couleur", 0.0f, 1.5f, 0.02f},
    {"Rayon shell", 0.3f, 2.0f, 0.02f},
    {"Motion smear", 0.0f, 1.0f, 0.02f},
    {"Visibilite sprite", 0.0f, 1.0f, 0.02f},
    {"Boost sprite", 0.0f, 1.5f, 0.03f},
    {"Teinte boost sprite", 0.0f, 1.0f, 0.02f},
}};

constexpr std::array<SliderSpec, kTrajectorySliderCount> kTrajectorySliderSpecs = {{
    {"Arc Bezier", 0.0f, 0.8f, 0.01f},
    {"Cote arc", -1.0f, 1.0f, 0.02f},
    {"Lobe ecran px", 0.0f, 220.0f, 2.0f},
    {"Intervalle salve", 0.05f, 2.5f, 0.03f},
    {"Sortie cote", -2.0f, 2.0f, 0.03f},
    {"Sortie avant", -2.0f, 2.0f, 0.03f},
    {"Eventail depart", 0.0f, 2.0f, 0.03f},
    {"Bruit depart", 0.0f, 0.8f, 0.02f},
    {"Spread impact", 0.0f, 2.0f, 0.03f},
    {"Relache file", 0.0f, 1.5f, 0.03f},
    {"Duree vol", 0.35f, 2.5f, 0.03f},
    {"Rythme vol", 0.0f, 1.0f, 0.02f},
}};

constexpr SliderSpec kShipSpeedSliderSpec = {
    "Vitesse navires",
    1.0f,
    14.0f,
    0.25f
};

constexpr std::array<const char*, kLayerButtonCount> kLayerLabels = {{
    "Outer",
    "Mid",
    "Inner",
}};

constexpr std::array<const char*, kPageButtonCount> kPageLabels = {{
    "Glow",
    "Trajectoire",
    "Trail",
}};

constexpr std::array<const char*, kDistanceBandButtonCount> kDistanceBandLabels = {{
    "0-10",
    "10-20",
    "20-40",
    "40+",
}};

constexpr std::array<const char*, kAngleSectorButtonCount> kAngleSectorLabels = {{
    "0-69",
    "70-110",
    "111-180",
    "181-250",
    "251-290",
    "291-0",
}};

constexpr std::array<SliderSpec, kTrailSliderCount> kTrailSliderSpecs = {{
    {"Longueur max tuiles", 0.15f, 32.0f, 0.15f},
    {"Largeur tete px", 1.0f, 256.0f, 2.0f},
    {"Largeur fin px", 0.0f, 256.0f, 2.0f},
    {"Courbe largeur", 0.2f, 4.0f, 0.05f},
    {"Opacite tete", 0.0f, 1.0f, 0.02f},
    {"Opacite fin", 0.0f, 1.0f, 0.02f},
    {"Courbe opacite", 0.2f, 4.0f, 0.05f},
    {"Tete rouge", 0.0f, 255.0f, 2.0f},
    {"Tete vert", 0.0f, 255.0f, 2.0f},
    {"Tete bleu", 0.0f, 255.0f, 2.0f},
    {"Fin rouge", 0.0f, 255.0f, 2.0f},
    {"Fin vert", 0.0f, 255.0f, 2.0f},
    {"Fin bleu", 0.0f, 255.0f, 2.0f},
    {"Distance min tir", 0.0f, 32.0f, 0.25f},
    {"Enrobe boulet", 0.0f, 4.0f, 0.04f},
    {"Spacing sprites", 0.05f, 4.0f, 0.04f},
    {"Desync sprites", 0.0f, 2.0f, 0.02f},
    {"Scale sprites", 0.05f, 6.0f, 0.05f},
    {"Alpha sprites tete", 0.0f, 1.0f, 0.02f},
    {"Alpha sprites fin", 0.0f, 1.0f, 0.02f},
    {"Teinte sprites", 0.0f, 1.0f, 0.02f},
}};

constexpr std::array<HelpEntry, kTrailSliderCount> kTrailSliderHelp = {{
    {"Longueur max tuiles", "Longueur maximale conservee derriere le boulet. La trainee grandit avec la distance parcourue puis se coupe a cette valeur pour eviter une trainée trop longue."},
    {"Largeur tete px", "Largeur du ribbon juste derriere le boulet. C'est la partie la plus visible du trail et celle qui donne le poids du projectile."},
    {"Largeur fin px", "Largeur au bout de la trainee. Baisse-la pour obtenir une fin plus fine et plus elegante, monte-la pour garder une trainée epaisse plus longtemps."},
    {"Courbe largeur", "Controle la vitesse a laquelle la largeur retrecit entre la tete et la fin. Valeur faible = transition douce, valeur forte = la fin devient fine plus vite."},
    {"Opacite tete", "Alpha du mesh pres du boulet. Sert a donner de la presence a la trainée au depart du projectile."},
    {"Opacite fin", "Alpha du mesh tout au bout de la trainee. Pratique pour faire une extinction progressive au lieu d'une coupe visuelle trop nette."},
    {"Courbe opacite", "Controle la facon dont l'opacite descend sur la longueur. Valeur haute = la trainée reste dense pres du boulet puis chute plus tard."},
    {"Tete rouge", "Canal rouge de la couleur pres du boulet. Combine ce trio RGB avec les trois canaux de fin pour construire un degradé complet."},
    {"Tete vert", "Canal vert de la couleur pres du boulet. Monte-le pour des teintes plus chaudes ou plus lumineuses."},
    {"Tete bleu", "Canal bleu de la couleur pres du boulet. Monte-le pour refroidir la tete de la trainée."},
    {"Fin rouge", "Canal rouge en fin de trainee. Utilise une fin plus claire ou plus desaturee pour faire un joli fondu aerien."},
    {"Fin vert", "Canal vert en fin de trainee. Tres utile pour equilibrer la lumiere de la fin sans casser la teinte de tete."},
    {"Fin bleu", "Canal bleu en fin de trainee. Aide a tirer le bout de trainée vers une fumee froide ou une poussiere plus pale."},
    {"Distance min tir", "Le trail ne s'affiche que si la distance totale du tir au moment du depart depasse ce seuil. Exemple: seuil 10, tir a 15 tuiles = trail visible jusqu'a la cible; tir a 8 tuiles = pas de trail du tout. Mets 0 pour desactiver."},
    {"Enrobe boulet", "Prolonge la tete du trail autour et legerement devant le boulet. Monte cette valeur si tu veux que le projectile soit visuellement dans la trainée plutot que juste en tete."},
    {"Spacing sprites", "Distance entre chaque stamp spritesheet pose sur le ribbon. Plus bas = plus de sprites et une trainée plus riche. Plus haut = trainée plus aeree."},
    {"Scale sprites", "Echelle globale des sprites poses sur la trainée. Sert a faire des rep repetes fins ou au contraire de grosses nappes animees."},
    {"Desync sprites", "Ajoute un decalage de phase d'animation entre les stamps successifs. Monte cette valeur pour eviter que toutes les particules jouent exactement la meme frame au meme moment."},
    {"Alpha sprites tete", "Opacite des stamps pres du boulet. Tu peux garder un mesh discret et faire porter le detail visuel par la spritesheet."},
    {"Alpha sprites fin", "Opacite des stamps en fin de trainee. Mets-la bas pour que la queue disparaisse en douceur."},
    {"Teinte sprites", "0 = sprites proches du blanc d'origine, 1 = sprites qui suivent fortement le degradé couleur du ribbon. Ideal pour marier texture et mesh."},
}};

static const HelpEntry& getTrailSliderHelpEntry(int sliderIndex)
{
    if (sliderIndex < 0 || sliderIndex >= kTrailSliderCount)
    {
        return kTrailSliderHelp[0];
    }

    if (sliderIndex == 16)
    {
        return kTrailSliderHelp[static_cast<size_t>(17)];
    }
    if (sliderIndex == 17)
    {
        return kTrailSliderHelp[static_cast<size_t>(16)];
    }
    return kTrailSliderHelp[static_cast<size_t>(sliderIndex)];
}

constexpr HelpEntry kTrailHelpDefault = {
    "Aide trail",
    "Survole un bouton ou un slider pour voir ce qu'il pilote exactement. La preview a droite montre le resultat en direct sans attendre un nouveau tir."
};
constexpr HelpEntry kTrailHelpTrailToggle = {
    "Trail ON / OFF",
    "Active ou coupe la trainée globale du preset courant. Le bouton principal force ensemble le mesh et les sprites, utile pour verifier rapidement le rendu avec ou sans trail."
};
constexpr HelpEntry kTrailHelpMeshToggle = {
    "Mesh ON / OFF",
    "Active le ruban geometrique colore. C'est lui qui donne la silhouette continue, la largeur et le degrade principal de la trainée."
};
constexpr HelpEntry kTrailHelpSpritesToggle = {
    "Sprites ON / OFF",
    "Active la pose repetee de la spritesheet le long du ribbon. Les sprites peuvent tourner et s'animer a plusieurs endroits en meme temps pour enrichir la trainée."
};
constexpr HelpEntry kTrailHelpBlendToggle = {
    "Blend sprites",
    "Choisit le blend des stamps trail. ADD renforce le cote lumineux et explosif, ALPHA garde un rendu plus mat et controle."
};
constexpr HelpEntry kTrailHelpImport = {
    "Import JSON",
    "Recharge la config trail depuis le fichier gameplay exporte. Pratique pour comparer un preset sauvegarde avec tes reglages live du moment."
};
constexpr HelpEntry kTrailHelpExport = {
    "Export JSON",
    "Sauvegarde les reglages trail actuels vers le JSON de trajectoire. Le runtime des salves pourra ainsi reutiliser le meme preset proprement."
};
constexpr HelpEntry kTrailHelpReset = {
    "Reset trail",
    "Remet les reglages ribbon trail par defaut. Ideal quand un preset est parti trop loin et qu'il faut repartir sur une base saine."
};
constexpr HelpEntry kTrailHelpHide = {
    "Masquer panneau",
    "Referme rapidement le panneau debug sans perdre les reglages deja appliques. Raccourci identique au bouton F2."
};
constexpr HelpEntry kTrailHelpPreview = {
    "Preview live",
    "Apercu embarque du boulet et de sa trainée actuelle. Le ruban grandit puis se coupe a la longueur max, et les sprites trail se repete/animent comme en jeu."
};

constexpr HelpEntry kTrailHelpManualPreview = {
    "Placement manuel",
    "Deuxieme preview en longueur maximale. Clique dans la courbe pour poser un sprite, deplace-le a la souris puis ajuste sa distance, son offset, sa taille, son opacite et sa rotation."
};
constexpr HelpEntry kTrailHelpManualMode = {
    "Mode manuel",
    "Quand il est actif, la couche sprites n'utilise plus le spacing automatique : seuls les stamps poses a la main sont rendus."
};
constexpr std::array<SliderSpec, kManualTrailSliderCount> kManualTrailSliderSpecs = {{
    {"Distance", -1.0f, 32.0f, 0.05f},
    {"Offset Y", -96.0f, 96.0f, 1.0f},
    {"Scale", 0.05f, 6.0f, 0.05f},
    {"Opacite", 0.0f, 1.0f, 0.02f},
    {"Rotation", -180.0f, 180.0f, 2.0f},
}};

struct IlluminatedProjectileDebugPanelLayout
{
    SDL_FRect panelRect{};
    SDL_FRect headerRect{};
    SDL_FRect hideButtonRect{};
    SDL_FRect badgeRect{};
    std::array<SDL_FRect, kPageButtonCount> pageButtonRects{};
    std::array<SDL_FRect, kSliderCount> rowRects{};
    std::array<SDL_FRect, kSliderCount> trackRects{};
    SDL_FRect distanceBandRowRect{};
    SDL_FRect shipSpeedRowRect{};
    SDL_FRect shipSpeedTrackRect{};
    std::array<SDL_FRect, kDistanceBandButtonCount> distanceBandButtonRects{};
    SDL_FRect angleSectorRowRect{};
    std::array<SDL_FRect, kAngleSectorButtonCount> angleSectorButtonRects{};
    std::array<SDL_FRect, kTrajectorySliderCount> trajectoryRowRects{};
    std::array<SDL_FRect, kTrajectorySliderCount> trajectoryTrackRects{};
    std::array<SDL_FRect, kTrajectorySliderCount> trajectoryDebugButtonRects{};
    SDL_FRect trajectoryImportButtonRect{};
    SDL_FRect trajectoryExportButtonRect{};
    SDL_FRect trajectoryResetSelectedButtonRect{};
    SDL_FRect trajectoryResetAllButtonRect{};
    SDL_FRect trajectoryCircleToggleButtonRect{};
    std::array<SDL_FRect, kTrailSliderCount> trailRowRects{};
    std::array<SDL_FRect, kTrailSliderCount> trailTrackRects{};
    SDL_FRect trailEnableButtonRect{};
    SDL_FRect trailMeshToggleButtonRect{};
    SDL_FRect trailStampsToggleButtonRect{};
    SDL_FRect trailBlendToggleButtonRect{};
    SDL_FRect trailImportButtonRect{};
    SDL_FRect trailExportButtonRect{};
    SDL_FRect trailResetButtonRect{};
    SDL_FRect trailFooterHideButtonRect{};
    SDL_FRect trailPreviewRect{};
    SDL_FRect trailManualModeButtonRect{};
    SDL_FRect trailManualAddButtonRect{};
    SDL_FRect trailManualDuplicateButtonRect{};
    SDL_FRect trailManualDeleteButtonRect{};
    SDL_FRect trailManualClearButtonRect{};
    SDL_FRect trailManualPreviewRect{};
    std::array<SDL_FRect, kManualTrailSliderCount> trailManualSliderRowRects{};
    std::array<SDL_FRect, kManualTrailSliderCount> trailManualSliderTrackRects{};
    SDL_FRect trailHelpRect{};
    SDL_FRect trailControlsViewportRect{};
    SDL_FRect trailScrollTrackRect{};
    SDL_FRect trailScrollThumbRect{};
    float trailScrollMaxOffset = 0.0f;
    SDL_FRect layerRowRect{};
    std::array<SDL_FRect, kLayerButtonCount> layerButtonRects{};
    SDL_FRect compactRowRect{};
    SDL_FRect compactToggleRect{};
    SDL_FRect glowImportButtonRect{};
    SDL_FRect glowExportButtonRect{};
    SDL_FRect resetButtonRect{};
    SDL_FRect footerHideButtonRect{};
};

static SDL_FRect getPanelScreenRect(void)
{
    SDL_FRect screenRect = GetGameScreen().rect;
    if (screenRect.w <= 0.0f || screenRect.h <= 0.0f)
    {
        screenRect = rc2d_engine_getVisibleSafeRectRender();
    }
    return screenRect;
}

static bool pointInRect(float x, float y, const SDL_FRect& rect)
{
    return (
        rect.w > 0.0f &&
        rect.h > 0.0f &&
        x >= rect.x &&
        x <= (rect.x + rect.w) &&
        y >= rect.y &&
        y <= (rect.y + rect.h));
}

static bool rectsIntersect(const SDL_FRect& a, const SDL_FRect& b)
{
    return
        a.w > 0.0f &&
        a.h > 0.0f &&
        b.w > 0.0f &&
        b.h > 0.0f &&
        a.x < (b.x + b.w) &&
        (a.x + a.w) > b.x &&
        a.y < (b.y + b.h) &&
        (a.y + a.h) > b.y;
}

static SDL_FRect clampRectInside(const SDL_FRect& rect, const SDL_FRect& bounds)
{
    if (bounds.w <= 0.0f || bounds.h <= 0.0f)
    {
        return rect;
    }

    SDL_FRect clamped = rect;
    if (clamped.w >= bounds.w)
    {
        clamped.x = bounds.x;
    }
    else
    {
        clamped.x = std::clamp(clamped.x, bounds.x, bounds.x + bounds.w - clamped.w);
    }

    if (clamped.h >= bounds.h)
    {
        clamped.y = bounds.y;
    }
    else
    {
        clamped.y = std::clamp(clamped.y, bounds.y, bounds.y + bounds.h - clamped.h);
    }

    return clamped;
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

static float measureTextWidth(RC2D_Font* font, const char* text)
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
    (void)height;
    return static_cast<float>(width);
}

static float measureTextHeight(RC2D_Font* font, const char* text)
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

static void drawTextAt(RC2D_Font* font, const char* text, float x, float y, RC2D_Color color)
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

static void drawCenteredText(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
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

static void drawRightAlignedText(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text == nullptr || text[0] == '\0')
    {
        return;
    }

    const float textWidth = measureTextWidth(font, text);
    const float textHeight = measureTextHeight(font, text);
    drawTextAt(
        font,
        text,
        rect.x + rect.w - textWidth,
        rect.y + ((rect.h - textHeight) * 0.5f),
        color);
}

static void fillAndOutlineRect(const SDL_FRect& rect, RC2D_Color fill, RC2D_Color border)
{
    rc2d_graphics_setColor(fill);
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setColor(border);
    rc2d_graphics_rectangle("line", &rect);
}

static SDL_Rect toClipRect(const SDL_FRect& rect)
{
    SDL_Rect clipRect{};
    clipRect.x = static_cast<int>(std::floor(rect.x));
    clipRect.y = static_cast<int>(std::floor(rect.y));
    const int clipRight = static_cast<int>(std::ceil(rect.x + rect.w));
    const int clipBottom = static_cast<int>(std::ceil(rect.y + rect.h));
    clipRect.w = (std::max)(clipRight - clipRect.x, 1);
    clipRect.h = (std::max)(clipBottom - clipRect.y, 1);
    return clipRect;
}

static std::vector<std::string> wrapTextLines(
    RC2D_Font* font,
    const std::string& text,
    float maxWidth)
{
    std::vector<std::string> lines;
    if (text.empty())
    {
        return lines;
    }

    std::size_t paragraphStart = 0U;
    while (paragraphStart <= text.size())
    {
        const std::size_t paragraphEnd = text.find('\n', paragraphStart);
        const std::string paragraph =
            text.substr(paragraphStart, (paragraphEnd == std::string::npos) ? std::string::npos : (paragraphEnd - paragraphStart));
        if (paragraph.empty())
        {
            lines.emplace_back();
        }
        else
        {
            std::string currentLine;
            std::size_t wordStart = 0U;
            while (wordStart < paragraph.size())
            {
                while (wordStart < paragraph.size() && paragraph[wordStart] == ' ')
                {
                    ++wordStart;
                }
                if (wordStart >= paragraph.size())
                {
                    break;
                }

                std::size_t wordEnd = paragraph.find(' ', wordStart);
                if (wordEnd == std::string::npos)
                {
                    wordEnd = paragraph.size();
                }
                const std::string word = paragraph.substr(wordStart, wordEnd - wordStart);
                const std::string candidate =
                    currentLine.empty() ? word : (currentLine + " " + word);
                if (!currentLine.empty() &&
                    maxWidth > 0.0f &&
                    measureTextWidth(font, candidate.c_str()) > maxWidth)
                {
                    lines.push_back(currentLine);
                    currentLine = word;
                }
                else
                {
                    currentLine = candidate;
                }
                wordStart = wordEnd + 1U;
            }

            if (!currentLine.empty())
            {
                lines.push_back(currentLine);
            }
        }

        if (paragraphEnd == std::string::npos)
        {
            break;
        }
        paragraphStart = paragraphEnd + 1U;
    }

    return lines;
}

static void drawWrappedText(
    RC2D_Font* font,
    const std::string& text,
    const SDL_FRect& rect,
    RC2D_Color color,
    float lineGap)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty() || rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    const std::vector<std::string> lines = wrapTextLines(font, text, rect.w);
    const float lineHeight = (std::max)(measureTextHeight(font, "Ag"), 14.0f) + lineGap;
    float drawY = rect.y;
    for (const std::string& line : lines)
    {
        if (drawY + lineHeight > rect.y + rect.h + 0.5f)
        {
            break;
        }
        drawTextAt(font, line.c_str(), rect.x, drawY, color);
        drawY += lineHeight;
    }
}

static std::string getFolderLeafLabel(const std::string& path)
{
    if (path.empty())
    {
        return {};
    }

    const std::filesystem::path folderPath(path);
    const std::filesystem::path leaf = folderPath.filename();
    if (!leaf.empty())
    {
        return leaf.generic_string();
    }
    return folderPath.generic_string();
}

static IlluminatedProjectileDebugPanelLayout buildLayout(
    const SDL_FPoint& panelOffset,
    float trailScrollOffsetY)
{
    IlluminatedProjectileDebugPanelLayout layout{};
    const SDL_FRect screenRect = getPanelScreenRect();
    layout.panelRect = clampRectInside(
        SDL_FRect{
            screenRect.x + kPanelMargin + panelOffset.x,
            screenRect.y + kPanelMargin + panelOffset.y,
            (std::min)(kPanelWidth, screenRect.w - (kPanelMargin * 2.0f)),
            (std::min)(kPanelHeight, screenRect.h - (kPanelMargin * 2.0f))
        },
        screenRect);

    layout.headerRect = SDL_FRect{
        layout.panelRect.x + 12.0f,
        layout.panelRect.y + 12.0f,
        layout.panelRect.w - 24.0f,
        kHeaderHeight
    };

    layout.hideButtonRect = SDL_FRect{
        layout.headerRect.x + layout.headerRect.w - 72.0f,
        layout.headerRect.y + 4.0f,
        68.0f,
        layout.headerRect.h - 8.0f
    };

    layout.badgeRect = SDL_FRect{
        layout.headerRect.x + layout.headerRect.w - 154.0f,
        layout.headerRect.y + 4.0f,
        74.0f,
        layout.headerRect.h - 8.0f
    };

    const float contentX = layout.panelRect.x + 16.0f;
    const float rowWidth = layout.panelRect.w - 32.0f;

    const float pageButtonsY = layout.panelRect.y + 56.0f;
    for (int index = 0; index < kPageButtonCount; ++index)
    {
        layout.pageButtonRects[static_cast<size_t>(index)] = SDL_FRect{
            contentX + (static_cast<float>(index) * (kPageButtonWidth + 8.0f)),
            pageButtonsY,
            kPageButtonWidth,
            kButtonHeight
        };
    }

    const float rowsStartY = layout.panelRect.y + 136.0f;
    const float trackX = contentX + 208.0f;
    const float valueX = trackX + kSliderTrackWidth + 14.0f;
    for (int index = 0; index < kSliderCount; ++index)
    {
        layout.rowRects[static_cast<size_t>(index)] = SDL_FRect{
            contentX,
            rowsStartY + (static_cast<float>(index) * (kSliderRowHeight + kSliderRowGap)),
            rowWidth,
            kSliderRowHeight
        };
        layout.trackRects[static_cast<size_t>(index)] = SDL_FRect{
            trackX,
            layout.rowRects[static_cast<size_t>(index)].y + ((kSliderRowHeight - kSliderTrackHeight) * 0.5f),
            (std::min)(kSliderTrackWidth, (std::max)(60.0f, rowWidth - 270.0f)),
            kSliderTrackHeight
        };
        (void)valueX;
    }

    layout.layerRowRect = SDL_FRect{
        contentX,
        layout.rowRects.back().y + kSliderRowHeight + 16.0f,
        rowWidth,
        kSliderRowHeight
    };

    const float layerButtonsStartX =
        layout.layerRowRect.x + layout.layerRowRect.w - ((kLayerButtonWidth * 3.0f) + 16.0f);
    for (int index = 0; index < kLayerButtonCount; ++index)
    {
        layout.layerButtonRects[static_cast<size_t>(index)] = SDL_FRect{
            layerButtonsStartX + (static_cast<float>(index) * (kLayerButtonWidth + 8.0f)),
            layout.layerRowRect.y,
            kLayerButtonWidth,
            layout.layerRowRect.h
        };
    }

    layout.compactRowRect = SDL_FRect{
        contentX,
        layout.layerRowRect.y + kSliderRowHeight + 10.0f,
        rowWidth,
        kSliderRowHeight
    };
    layout.compactToggleRect = SDL_FRect{
        layout.compactRowRect.x + layout.compactRowRect.w - 138.0f,
        layout.compactRowRect.y,
        138.0f,
        layout.compactRowRect.h
    };

    layout.shipSpeedRowRect = SDL_FRect{
        contentX,
        layout.panelRect.y + 116.0f,
        rowWidth,
        kSliderRowHeight
    };
    layout.shipSpeedTrackRect = SDL_FRect{
        trackX,
        layout.shipSpeedRowRect.y + ((kSliderRowHeight - kSliderTrackHeight) * 0.5f),
        (std::min)(kSliderTrackWidth, (std::max)(60.0f, rowWidth - 270.0f)),
        kSliderTrackHeight
    };

    layout.distanceBandRowRect = SDL_FRect{
        contentX,
        layout.shipSpeedRowRect.y + kSliderRowHeight + 10.0f,
        rowWidth,
        kSliderRowHeight
    };
    const float distanceButtonsStartX =
        layout.distanceBandRowRect.x + layout.distanceBandRowRect.w -
        ((kDistanceBandButtonWidth * static_cast<float>(kDistanceBandButtonCount)) +
         (8.0f * static_cast<float>(kDistanceBandButtonCount - 1)));
    for (int index = 0; index < kDistanceBandButtonCount; ++index)
    {
        layout.distanceBandButtonRects[static_cast<size_t>(index)] = SDL_FRect{
            distanceButtonsStartX + (static_cast<float>(index) * (kDistanceBandButtonWidth + 8.0f)),
            layout.distanceBandRowRect.y,
            kDistanceBandButtonWidth,
            layout.distanceBandRowRect.h
        };
    }

    layout.angleSectorRowRect = SDL_FRect{
        contentX,
        layout.distanceBandRowRect.y + kSliderRowHeight + 10.0f,
        rowWidth,
        kSliderRowHeight
    };
    const float angleButtonsStartX =
        layout.angleSectorRowRect.x + layout.angleSectorRowRect.w -
        ((kAngleSectorButtonWidth * static_cast<float>(kAngleSectorButtonCount)) +
         (6.0f * static_cast<float>(kAngleSectorButtonCount - 1)));
    for (int index = 0; index < kAngleSectorButtonCount; ++index)
    {
        layout.angleSectorButtonRects[static_cast<size_t>(index)] = SDL_FRect{
            angleButtonsStartX + (static_cast<float>(index) * (kAngleSectorButtonWidth + 6.0f)),
            layout.angleSectorRowRect.y,
            kAngleSectorButtonWidth,
            layout.angleSectorRowRect.h
        };
    }

    const float trajectoryRowsStartY = layout.angleSectorRowRect.y + kSliderRowHeight + 16.0f;
    const float trajectoryTrackWidth =
        (std::min)(
            kSliderTrackWidth,
            (std::max)(
                60.0f,
                rowWidth -
                    208.0f -
                    kTrajectoryDebugButtonGap -
                    kTrajectoryDebugButtonWidth -
                    8.0f -
                    70.0f));
    for (int index = 0; index < kTrajectorySliderCount; ++index)
    {
        layout.trajectoryRowRects[static_cast<size_t>(index)] = SDL_FRect{
            contentX,
            trajectoryRowsStartY + (static_cast<float>(index) * (kSliderRowHeight + kSliderRowGap)),
            rowWidth,
            kSliderRowHeight
        };
        layout.trajectoryTrackRects[static_cast<size_t>(index)] = SDL_FRect{
            trackX,
            layout.trajectoryRowRects[static_cast<size_t>(index)].y + ((kSliderRowHeight - kSliderTrackHeight) * 0.5f),
            trajectoryTrackWidth,
            kSliderTrackHeight
        };
        layout.trajectoryDebugButtonRects[static_cast<size_t>(index)] = SDL_FRect{
            layout.trajectoryTrackRects[static_cast<size_t>(index)].x +
                layout.trajectoryTrackRects[static_cast<size_t>(index)].w +
                kTrajectoryDebugButtonGap,
            layout.trajectoryRowRects[static_cast<size_t>(index)].y,
            kTrajectoryDebugButtonWidth,
            layout.trajectoryRowRects[static_cast<size_t>(index)].h
        };
    }

    layout.trajectoryExportButtonRect = SDL_FRect{
        contentX,
        layout.panelRect.y + layout.panelRect.h - kButtonHeight - 16.0f,
        158.0f,
        kButtonHeight
    };
    layout.trajectoryResetSelectedButtonRect = SDL_FRect{
        layout.trajectoryExportButtonRect.x + layout.trajectoryExportButtonRect.w + 10.0f,
        layout.trajectoryExportButtonRect.y,
        118.0f,
        kButtonHeight
    };
    layout.trajectoryResetAllButtonRect = SDL_FRect{
        layout.trajectoryResetSelectedButtonRect.x + layout.trajectoryResetSelectedButtonRect.w + 10.0f,
        layout.trajectoryExportButtonRect.y,
        92.0f,
        kButtonHeight
    };
    layout.trajectoryCircleToggleButtonRect = SDL_FRect{
        layout.trajectoryResetAllButtonRect.x + layout.trajectoryResetAllButtonRect.w + 10.0f,
        layout.trajectoryExportButtonRect.y,
        118.0f,
        kButtonHeight
    };

    float trailPreviewWidth =
        (std::clamp)(layout.panelRect.w * 0.31f, 220.0f, 336.0f);
    const float trailColumnGap = 16.0f;
    float trailControlsWidth = rowWidth - trailPreviewWidth - trailColumnGap;
    if (trailControlsWidth < 348.0f)
    {
        trailControlsWidth = (std::min)(348.0f, rowWidth);
        trailPreviewWidth = (std::max)(180.0f, rowWidth - trailControlsWidth - trailColumnGap);
    }
    const float trailPreviewX = contentX + trailControlsWidth + trailColumnGap;
    const float trailRowsStartY = layout.panelRect.y + 168.0f;
    const float trailVisibleBottomY = layout.panelRect.y + layout.panelRect.h - kButtonHeight - 52.0f;
    layout.trailControlsViewportRect = SDL_FRect{
        contentX,
        trailRowsStartY,
        trailControlsWidth - kScrollBarWidth - 8.0f,
        (std::max)(96.0f, trailVisibleBottomY - trailRowsStartY)
    };
    const float trailContentHeight =
        (static_cast<float>(kTrailSliderCount) * kSliderRowHeight) +
        (static_cast<float>((std::max)(0, kTrailSliderCount - 1)) * kSliderRowGap);
    layout.trailScrollMaxOffset =
        (std::max)(0.0f, trailContentHeight - layout.trailControlsViewportRect.h);
    const float clampedTrailScrollOffset =
        std::clamp(trailScrollOffsetY, 0.0f, layout.trailScrollMaxOffset);
    layout.trailScrollTrackRect = SDL_FRect{
        layout.trailControlsViewportRect.x + layout.trailControlsViewportRect.w + 8.0f,
        layout.trailControlsViewportRect.y,
        kScrollBarWidth,
        layout.trailControlsViewportRect.h
    };
    if (layout.trailScrollMaxOffset > 1.0e-4f)
    {
        const float visibleRatio =
            std::clamp(layout.trailControlsViewportRect.h / trailContentHeight, 0.0f, 1.0f);
        const float thumbHeight =
            (std::max)(30.0f, layout.trailScrollTrackRect.h * visibleRatio);
        const float thumbTravel = (std::max)(0.0f, layout.trailScrollTrackRect.h - thumbHeight);
        const float thumbT =
            (layout.trailScrollMaxOffset > 1.0e-4f) ? (clampedTrailScrollOffset / layout.trailScrollMaxOffset) : 0.0f;
        layout.trailScrollThumbRect = SDL_FRect{
            layout.trailScrollTrackRect.x,
            layout.trailScrollTrackRect.y + (thumbTravel * thumbT),
            layout.trailScrollTrackRect.w,
            thumbHeight
        };
    }
    else
    {
        layout.trailScrollThumbRect = layout.trailScrollTrackRect;
    }
    const float trailTrackWidth =
        (std::min)(
            kSliderTrackWidth,
            (std::max)(
                60.0f,
                layout.trailControlsViewportRect.w - 208.0f - 70.0f));
    for (int index = 0; index < kTrailSliderCount; ++index)
    {
        layout.trailRowRects[static_cast<size_t>(index)] = SDL_FRect{
            contentX,
            trailRowsStartY + (static_cast<float>(index) * (kSliderRowHeight + kSliderRowGap)) - clampedTrailScrollOffset,
            layout.trailControlsViewportRect.w,
            kSliderRowHeight
        };
        layout.trailTrackRects[static_cast<size_t>(index)] = SDL_FRect{
            trackX,
            layout.trailRowRects[static_cast<size_t>(index)].y + ((kSliderRowHeight - kSliderTrackHeight) * 0.5f),
            trailTrackWidth,
            kSliderTrackHeight
        };
    }

    const float trailButtonsY = layout.panelRect.y + 116.0f;
    layout.trailEnableButtonRect = SDL_FRect{contentX, trailButtonsY, 138.0f, kButtonHeight};
    layout.trailMeshToggleButtonRect = SDL_FRect{contentX + 148.0f, trailButtonsY, 138.0f, kButtonHeight};
    layout.trailStampsToggleButtonRect = SDL_FRect{contentX + 296.0f, trailButtonsY, 138.0f, kButtonHeight};
    layout.trailBlendToggleButtonRect = SDL_FRect{contentX + 444.0f, trailButtonsY, 138.0f, kButtonHeight};
    const float trailInfoBottomY = layout.panelRect.y + layout.panelRect.h - kButtonHeight - 28.0f;
    const float trailPreviewHeight = 178.0f;
    layout.trailPreviewRect = SDL_FRect{
        trailPreviewX,
        trailButtonsY,
        layout.panelRect.x + layout.panelRect.w - trailPreviewX - 16.0f,
        trailPreviewHeight
    };
    const float manualButtonsY = layout.trailPreviewRect.y + layout.trailPreviewRect.h + 10.0f;
    const float manualButtonGap = 8.0f;
    const float manualButtonsTotalGap = manualButtonGap * 4.0f;
    const float manualButtonsWidth = (std::max)(220.0f, layout.trailPreviewRect.w);
    const float manualCompactButtonWidth =
        (std::clamp)((manualButtonsWidth - 126.0f - manualButtonsTotalGap) / 4.0f, 52.0f, 92.0f);
    layout.trailManualModeButtonRect = SDL_FRect{trailPreviewX, manualButtonsY, 126.0f, kButtonHeight};
    const float manualButtonsStartX = layout.trailManualModeButtonRect.x + layout.trailManualModeButtonRect.w + manualButtonGap;
    layout.trailManualAddButtonRect = SDL_FRect{manualButtonsStartX, manualButtonsY, manualCompactButtonWidth, kButtonHeight};
    layout.trailManualDuplicateButtonRect = SDL_FRect{
        layout.trailManualAddButtonRect.x + layout.trailManualAddButtonRect.w + manualButtonGap,
        manualButtonsY,
        manualCompactButtonWidth,
        kButtonHeight
    };
    layout.trailManualDeleteButtonRect = SDL_FRect{
        layout.trailManualDuplicateButtonRect.x + layout.trailManualDuplicateButtonRect.w + manualButtonGap,
        manualButtonsY,
        manualCompactButtonWidth,
        kButtonHeight
    };
    layout.trailManualClearButtonRect = SDL_FRect{
        layout.trailManualDeleteButtonRect.x + layout.trailManualDeleteButtonRect.w + manualButtonGap,
        manualButtonsY,
        manualCompactButtonWidth,
        kButtonHeight
    };
    const float manualPreviewY = manualButtonsY + kButtonHeight + 10.0f;
    layout.trailManualPreviewRect = SDL_FRect{
        trailPreviewX,
        manualPreviewY,
        layout.trailPreviewRect.w,
        168.0f
    };
    const float manualSliderTrackWidth =
        (std::min)(190.0f, (std::max)(80.0f, layout.trailManualPreviewRect.w - 154.0f));
    for (int index = 0; index < kManualTrailSliderCount; ++index)
    {
        layout.trailManualSliderRowRects[static_cast<size_t>(index)] = SDL_FRect{
            trailPreviewX,
            layout.trailManualPreviewRect.y + layout.trailManualPreviewRect.h + 10.0f +
                (static_cast<float>(index) * (kSliderRowHeight + 6.0f)),
            layout.trailManualPreviewRect.w,
            kSliderRowHeight
        };
        layout.trailManualSliderTrackRects[static_cast<size_t>(index)] = SDL_FRect{
            trailPreviewX + 132.0f,
            layout.trailManualSliderRowRects[static_cast<size_t>(index)].y + ((kSliderRowHeight - kSliderTrackHeight) * 0.5f),
            manualSliderTrackWidth,
            kSliderTrackHeight
        };
    }
    layout.trailHelpRect = SDL_FRect{
        trailPreviewX,
        layout.trailManualSliderRowRects.back().y + layout.trailManualSliderRowRects.back().h + 10.0f,
        layout.trailPreviewRect.w,
        trailInfoBottomY -
            (layout.trailManualSliderRowRects.back().y + layout.trailManualSliderRowRects.back().h + 10.0f)
    };
    layout.trailImportButtonRect = SDL_FRect{
        contentX,
        layout.panelRect.y + layout.panelRect.h - kButtonHeight - 16.0f,
        158.0f,
        kButtonHeight
    };
    layout.trailExportButtonRect = SDL_FRect{
        layout.trailImportButtonRect.x + layout.trailImportButtonRect.w + 10.0f,
        layout.trailImportButtonRect.y,
        158.0f,
        kButtonHeight
    };
    layout.trailResetButtonRect = SDL_FRect{
        layout.trailExportButtonRect.x + layout.trailExportButtonRect.w + 10.0f,
        layout.trailExportButtonRect.y,
        152.0f,
        kButtonHeight
    };
    layout.trailFooterHideButtonRect = SDL_FRect{
        layout.panelRect.x + layout.panelRect.w - kHideButtonWidth - 16.0f,
        layout.trailResetButtonRect.y,
        kHideButtonWidth,
        kButtonHeight
    };

    layout.glowImportButtonRect = SDL_FRect{
        contentX,
        layout.panelRect.y + layout.panelRect.h - kButtonHeight - 16.0f,
        158.0f,
        kButtonHeight
    };
    layout.glowExportButtonRect = SDL_FRect{
        layout.glowImportButtonRect.x + layout.glowImportButtonRect.w + 10.0f,
        layout.glowImportButtonRect.y,
        158.0f,
        kButtonHeight
    };
    layout.resetButtonRect = SDL_FRect{
        layout.glowExportButtonRect.x + layout.glowExportButtonRect.w + 10.0f,
        layout.glowExportButtonRect.y,
        kResetButtonWidth,
        kButtonHeight
    };
    layout.footerHideButtonRect = SDL_FRect{
        layout.panelRect.x + layout.panelRect.w - kHideButtonWidth - 16.0f,
        layout.resetButtonRect.y,
        kHideButtonWidth,
        kButtonHeight
    };

    return layout;
}

static float getSliderValue(
    const VFXClassic::IlluminatedProjectileGlowConfig& config,
    int sliderIndex)
{
    switch (sliderIndex)
    {
        case 0:
            return config.glowIntensity;
        case 1:
            return config.glowOpacity;
        case 2:
            return config.glowRadius;
        case 3:
            return config.glowDispersion;
        case 4:
            return config.glowRoundness;
        case 5:
            return config.coreSharpness;
        case 6:
            return config.centerIntensity;
        case 7:
            return config.centerFlashReduction;
        case 8:
            return config.edgeSoftness;
        case 9:
            return config.coreWhiteIntensity;
        case 10:
            return config.coreWhiteRadius;
        case 11:
            return config.colorShellIntensity;
        case 12:
            return config.colorShellRadius;
        case 13:
            return config.motionSmear;
        case 14:
            return config.spriteOpacity;
        case 15:
            return config.spriteBoost;
        case 16:
            return config.spriteTintStrength;
        default:
            return 0.0f;
    }
}

static void setSliderValue(
    VFXClassic::IlluminatedProjectileGlowConfig* config,
    int sliderIndex,
    float value)
{
    if (config == nullptr)
    {
        return;
    }

    switch (sliderIndex)
    {
        case 0:
            config->glowIntensity = value;
            break;
        case 1:
            config->glowOpacity = value;
            break;
        case 2:
            config->glowRadius = value;
            break;
        case 3:
            config->glowDispersion = value;
            break;
        case 4:
            config->glowRoundness = value;
            break;
        case 5:
            config->coreSharpness = value;
            break;
        case 6:
            config->centerIntensity = value;
            break;
        case 7:
            config->centerFlashReduction = value;
            break;
        case 8:
            config->edgeSoftness = value;
            break;
        case 9:
            config->coreWhiteIntensity = value;
            break;
        case 10:
            config->coreWhiteRadius = value;
            break;
        case 11:
            config->colorShellIntensity = value;
            break;
        case 12:
            config->colorShellRadius = value;
            break;
        case 13:
            config->motionSmear = value;
            break;
        case 14:
            config->spriteOpacity = value;
            break;
        case 15:
            config->spriteBoost = value;
            break;
        case 16:
            config->spriteTintStrength = value;
            break;
        default:
            break;
    }
}

static bool getLayerEnabled(
    const VFXClassic::IlluminatedProjectileGlowConfig& config,
    int layerIndex)
{
    switch (layerIndex)
    {
        case 0:
            return config.outerLayerEnabled;
        case 1:
            return config.midLayerEnabled;
        case 2:
            return config.innerLayerEnabled;
        default:
            return false;
    }
}

static void setLayerEnabled(
    VFXClassic::IlluminatedProjectileGlowConfig* config,
    int layerIndex,
    bool enabled)
{
    if (config == nullptr)
    {
        return;
    }

    switch (layerIndex)
    {
        case 0:
            config->outerLayerEnabled = enabled;
            break;
        case 1:
            config->midLayerEnabled = enabled;
            break;
        case 2:
            config->innerLayerEnabled = enabled;
            break;
        default:
            break;
    }
}

static bool getCompactGlowEnabled(const VFXClassic::IlluminatedProjectileGlowConfig& config)
{
    return config.compactGlowEnabled;
}

static void setCompactGlowEnabled(
    VFXClassic::IlluminatedProjectileGlowConfig* config,
    bool enabled)
{
    if (config == nullptr)
    {
        return;
    }

    config->compactGlowEnabled = enabled;
}

static float getTrajectorySliderValue(
    const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning& tuning,
    int sliderIndex)
{
    switch (sliderIndex)
    {
        case 0:
            return tuning.bezierBowChordFraction;
        case 1:
            return tuning.arcSide;
        case 2:
            return tuning.screenLobPixels;
        case 3:
            return tuning.launchStaggerScale;
        case 4:
            return tuning.launchSideOffsetTiles;
        case 5:
            return tuning.launchForwardOffsetTiles;
        case 6:
            return tuning.launchFanHalfTiles;
        case 7:
            return tuning.launchNoiseTiles;
        case 8:
            return tuning.impactSpreadScale;
        case 9:
            return tuning.flightLaneJitterTiles;
        case 10:
            return tuning.flightDurationScale;
        case 11:
            return tuning.flightEaseStrength;
        default:
            return 0.0f;
    }
}

static void setTrajectorySliderValue(
    MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning* tuning,
    int sliderIndex,
    float value)
{
    if (tuning == nullptr)
    {
        return;
    }

    switch (sliderIndex)
    {
        case 0:
            tuning->bezierBowChordFraction = value;
            break;
        case 1:
            tuning->arcSide = (std::isfinite(value) && value < 0.0f) ? -1.0f : 1.0f;
            break;
        case 2:
            tuning->screenLobPixels = value;
            break;
        case 3:
            tuning->launchStaggerScale = value;
            break;
        case 4:
            tuning->launchSideOffsetTiles = value;
            break;
        case 5:
            tuning->launchForwardOffsetTiles = value;
            break;
        case 6:
            tuning->launchFanHalfTiles = value;
            break;
        case 7:
            tuning->launchNoiseTiles = value;
            break;
        case 8:
            tuning->impactSpreadScale = value;
            break;
        case 9:
            tuning->flightLaneJitterTiles = value;
            break;
        case 10:
            tuning->flightDurationScale = value;
            break;
        case 11:
            tuning->flightEaseStrength = value;
            break;
        default:
            break;
    }
}

static float getTrailSliderValue(
    const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig& config,
    int sliderIndex)
{
    switch (sliderIndex)
    {
        case 0:
            return config.maxLengthTiles;
        case 1:
            return config.headWidthPixels;
        case 2:
            return config.tailWidthPixels;
        case 3:
            return config.widthExponent;
        case 4:
            return config.headOpacity;
        case 5:
            return config.tailOpacity;
        case 6:
            return config.opacityExponent;
        case 7:
            return config.headColorR;
        case 8:
            return config.headColorG;
        case 9:
            return config.headColorB;
        case 10:
            return config.tailColorR;
        case 11:
            return config.tailColorG;
        case 12:
            return config.tailColorB;
        case 13:
            return config.hideNearTargetTiles;
        case 14:
            return config.headCoverTiles;
        case 15:
            return config.stampSpacingTiles;
        case 16:
            return config.stampPhaseOffsetSeconds;
        case 17:
            return config.stampScale;
        case 18:
            return config.stampHeadOpacity;
        case 19:
            return config.stampTailOpacity;
        case 20:
            return config.stampTintStrength;
        default:
            return 0.0f;
    }
}

static void setTrailSliderValue(
    MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig* config,
    int sliderIndex,
    float value)
{
    if (config == nullptr)
    {
        return;
    }

    switch (sliderIndex)
    {
        case 0:
            config->maxLengthTiles = value;
            break;
        case 1:
            config->headWidthPixels = value;
            break;
        case 2:
            config->tailWidthPixels = value;
            break;
        case 3:
            config->widthExponent = value;
            break;
        case 4:
            config->headOpacity = value;
            break;
        case 5:
            config->tailOpacity = value;
            break;
        case 6:
            config->opacityExponent = value;
            break;
        case 7:
            config->headColorR = value;
            break;
        case 8:
            config->headColorG = value;
            break;
        case 9:
            config->headColorB = value;
            break;
        case 10:
            config->tailColorR = value;
            break;
        case 11:
            config->tailColorG = value;
            break;
        case 12:
            config->tailColorB = value;
            break;
        case 13:
            config->hideNearTargetTiles = value;
            break;
        case 14:
            config->headCoverTiles = value;
            break;
        case 15:
            config->stampSpacingTiles = value;
            break;
        case 16:
            config->stampPhaseOffsetSeconds = value;
            break;
        case 17:
            config->stampScale = value;
            break;
        case 18:
            config->stampHeadOpacity = value;
            break;
        case 19:
            config->stampTailOpacity = value;
            break;
        case 20:
            config->stampTintStrength = value;
            break;
        default:
            break;
    }
}

static float getManualTrailSliderValue(
    const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig::CustomStampPlacement& stamp,
    int sliderIndex)
{
    switch (sliderIndex)
    {
        case 0:
            return stamp.distanceFromHeadTiles;
        case 1:
            return stamp.lateralOffsetPixels;
        case 2:
            return stamp.scale;
        case 3:
            return stamp.opacity;
        case 4:
            return stamp.rotationOffsetDeg;
        default:
            return 0.0f;
    }
}

static void setManualTrailSliderValue(
    MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig::CustomStampPlacement* stamp,
    int sliderIndex,
    float value)
{
    if (stamp == nullptr)
    {
        return;
    }

    switch (sliderIndex)
    {
        case 0:
            stamp->distanceFromHeadTiles = value;
            break;
        case 1:
            stamp->lateralOffsetPixels = value;
            break;
        case 2:
            stamp->scale = value;
            break;
        case 3:
            stamp->opacity = value;
            break;
        case 4:
            stamp->rotationOffsetDeg = value;
            break;
        default:
            break;
    }
}

static float sliderValueFromTrackPosition(int sliderIndex, float mouseX, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kSliderCount)
    {
        return 0.0f;
    }

    const SliderSpec& spec = kSliderSpecs[static_cast<size_t>(sliderIndex)];
    if (trackRect.w <= 0.0f)
    {
        return spec.minValue;
    }

    const float normalized = std::clamp((mouseX - trackRect.x) / trackRect.w, 0.0f, 1.0f);
    return spec.minValue + ((spec.maxValue - spec.minValue) * normalized);
}

static float trajectorySliderValueFromTrackPosition(int sliderIndex, float mouseX, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kTrajectorySliderCount)
    {
        return 0.0f;
    }

    const SliderSpec& spec = kTrajectorySliderSpecs[static_cast<size_t>(sliderIndex)];
    if (trackRect.w <= 0.0f)
    {
        return spec.minValue;
    }

    const float normalized = std::clamp((mouseX - trackRect.x) / trackRect.w, 0.0f, 1.0f);
    return spec.minValue + ((spec.maxValue - spec.minValue) * normalized);
}

static float trailSliderValueFromTrackPosition(int sliderIndex, float mouseX, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kTrailSliderCount)
    {
        return 0.0f;
    }

    const SliderSpec& spec = kTrailSliderSpecs[static_cast<size_t>(sliderIndex)];
    if (trackRect.w <= 0.0f)
    {
        return spec.minValue;
    }

    const float normalized = std::clamp((mouseX - trackRect.x) / trackRect.w, 0.0f, 1.0f);
    return spec.minValue + ((spec.maxValue - spec.minValue) * normalized);
}

static float shipSpeedSliderValueFromTrackPosition(float mouseX, const SDL_FRect& trackRect)
{
    if (trackRect.w <= 0.0f)
    {
        return kShipSpeedSliderSpec.minValue;
    }

    const float normalized = std::clamp((mouseX - trackRect.x) / trackRect.w, 0.0f, 1.0f);
    return kShipSpeedSliderSpec.minValue +
        ((kShipSpeedSliderSpec.maxValue - kShipSpeedSliderSpec.minValue) * normalized);
}

static SDL_FRect buildSliderKnobRect(int sliderIndex, float value, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kSliderCount)
    {
        return SDL_FRect{};
    }

    const SliderSpec& spec = kSliderSpecs[static_cast<size_t>(sliderIndex)];
    const float range = spec.maxValue - spec.minValue;
    const float normalized =
        (range > 0.0f) ? std::clamp((value - spec.minValue) / range, 0.0f, 1.0f) : 0.0f;
    const float knobCenterX = trackRect.x + (trackRect.w * normalized);
    return SDL_FRect{
        knobCenterX - (kSliderKnobWidth * 0.5f),
        trackRect.y - ((kSliderKnobHeight - trackRect.h) * 0.5f),
        kSliderKnobWidth,
        kSliderKnobHeight
    };
}

static SDL_FRect buildTrailSliderKnobRect(int sliderIndex, float value, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kTrailSliderCount)
    {
        return SDL_FRect{};
    }

    const SliderSpec& spec = kTrailSliderSpecs[static_cast<size_t>(sliderIndex)];
    const float range = spec.maxValue - spec.minValue;
    const float normalized =
        (range > 0.0f) ? std::clamp((value - spec.minValue) / range, 0.0f, 1.0f) : 0.0f;
    const float knobCenterX = trackRect.x + (trackRect.w * normalized);
    return SDL_FRect{
        knobCenterX - (kSliderKnobWidth * 0.5f),
        trackRect.y - ((kSliderKnobHeight - trackRect.h) * 0.5f),
        kSliderKnobWidth,
        kSliderKnobHeight
    };
}

static float manualTrailSliderValueFromTrackPosition(int sliderIndex, float mouseX, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kManualTrailSliderCount)
    {
        return 0.0f;
    }

    const SliderSpec& spec = kManualTrailSliderSpecs[static_cast<size_t>(sliderIndex)];
    if (trackRect.w <= 0.0f)
    {
        return spec.minValue;
    }

    const float t = std::clamp((mouseX - trackRect.x) / trackRect.w, 0.0f, 1.0f);
    return spec.minValue + ((spec.maxValue - spec.minValue) * t);
}

static SDL_FRect buildManualTrailSliderKnobRect(int sliderIndex, float value, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kManualTrailSliderCount)
    {
        return SDL_FRect{};
    }

    const SliderSpec& spec = kManualTrailSliderSpecs[static_cast<size_t>(sliderIndex)];
    const float range = spec.maxValue - spec.minValue;
    const float t =
        (range > 1.0e-5f)
            ? std::clamp((value - spec.minValue) / range, 0.0f, 1.0f)
            : 0.0f;
    return SDL_FRect{
        trackRect.x + (trackRect.w * t) - (kSliderKnobWidth * 0.5f),
        trackRect.y + ((trackRect.h - kSliderKnobHeight) * 0.5f),
        kSliderKnobWidth,
        kSliderKnobHeight
    };
}

static SDL_FRect buildTrajectorySliderKnobRect(int sliderIndex, float value, const SDL_FRect& trackRect)
{
    if (sliderIndex < 0 || sliderIndex >= kTrajectorySliderCount)
    {
        return SDL_FRect{};
    }

    const SliderSpec& spec = kTrajectorySliderSpecs[static_cast<size_t>(sliderIndex)];
    const float range = spec.maxValue - spec.minValue;
    const float normalized =
        (range > 0.0f) ? std::clamp((value - spec.minValue) / range, 0.0f, 1.0f) : 0.0f;
    const float knobCenterX = trackRect.x + (trackRect.w * normalized);
    return SDL_FRect{
        knobCenterX - (kSliderKnobWidth * 0.5f),
        trackRect.y - ((kSliderKnobHeight - trackRect.h) * 0.5f),
        kSliderKnobWidth,
        kSliderKnobHeight
    };
}

static SDL_FRect buildShipSpeedSliderKnobRect(float value, const SDL_FRect& trackRect)
{
    const float range = kShipSpeedSliderSpec.maxValue - kShipSpeedSliderSpec.minValue;
    const float normalized =
        (range > 0.0f) ? std::clamp((value - kShipSpeedSliderSpec.minValue) / range, 0.0f, 1.0f) : 0.0f;
    const float knobCenterX = trackRect.x + (trackRect.w * normalized);
    return SDL_FRect{
        knobCenterX - (kSliderKnobWidth * 0.5f),
        trackRect.y - ((kSliderKnobHeight - trackRect.h) * 0.5f),
        kSliderKnobWidth,
        kSliderKnobHeight
    };
}

static bool isArcSideControlIndex(int sliderIndex)
{
    return sliderIndex == 1;
}

static float normalizeArcSideSelection(float value)
{
    return (std::isfinite(value) && value < 0.0f) ? -1.0f : 1.0f;
}

static const char* getArcSideSelectionLabel(float value)
{
    return normalizeArcSideSelection(value) < 0.0f ? "Gauche" : "Droite";
}

static float getDebugShipSpeedTilesPerSecond(void)
{
    const GameState& gameState = GetGameState();
    return std::clamp(
        gameState.player.getMoveSpeedTilesPerSecond(),
        kShipSpeedSliderSpec.minValue,
        kShipSpeedSliderSpec.maxValue);
}

static void setDebugShipSpeedTilesPerSecond(float speed)
{
    const float clamped =
        std::clamp(speed, kShipSpeedSliderSpec.minValue, kShipSpeedSliderSpec.maxValue);

    GameState& gameState = GetGameState();
    gameState.player.setMoveSpeedTilesPerSecond(clamped);
    for (Player& player : gameState.otherPlayers)
    {
        player.setMoveSpeedTilesPerSecond(clamped);
    }
}

static int getArcSideSelectionSign(float value)
{
    return normalizeArcSideSelection(value) < 0.0f ? -1 : 1;
}

static void forceDebugOffsetToArcSide(
    float chordDx,
    float chordDy,
    int arcSideSign,
    float* offsetX,
    float* offsetY)
{
    if (offsetX == nullptr || offsetY == nullptr)
    {
        return;
    }

    const float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
    if (chordLen < 1.0e-5f)
    {
        return;
    }

    const float dirX = chordDx / chordLen;
    const float dirY = chordDy / chordLen;
    const float sideNormalX = (arcSideSign < 0) ? (chordDy / chordLen) : (-chordDy / chordLen);
    const float sideNormalY = (arcSideSign < 0) ? (-chordDx / chordLen) : (chordDx / chordLen);
    const float along = (*offsetX * dirX) + (*offsetY * dirY);
    const float sideDistance = (std::max)(0.0f, std::fabs((*offsetX * sideNormalX) + (*offsetY * sideNormalY)));
    *offsetX = (dirX * along) + (sideNormalX * sideDistance);
    *offsetY = (dirY * along) + (sideNormalY * sideDistance);
}

static float remapDebugFastSlowFastProgress(float u, float strength)
{
    const float clampedU = std::clamp((std::isfinite(u) ? u : 0.0f), 0.0f, 1.0f);
    const float s = std::clamp((std::isfinite(strength) ? strength : 0.0f), 0.0f, 1.0f);
    if (s <= 0.0001f)
    {
        return clampedU;
    }

    float fastSlowFast = clampedU;
    if (clampedU < 0.5f)
    {
        const float t = clampedU * 2.0f;
        fastSlowFast = 0.5f * (1.0f - ((1.0f - t) * (1.0f - t)));
    }
    else
    {
        const float t = (clampedU - 0.5f) * 2.0f;
        fastSlowFast = 0.5f + (0.5f * t * t);
    }

    return clampedU + ((fastSlowFast - clampedU) * s);
}

static SDL_FRect buildArcSideChoiceRect(const SDL_FRect& trackRect, bool rightChoice)
{
    const float gap = 8.0f;
    const float buttonWidth = (trackRect.w - gap) * 0.5f;
    return SDL_FRect{
        trackRect.x + (rightChoice ? (buttonWidth + gap) : 0.0f),
        trackRect.y - ((kSliderRowHeight - trackRect.h) * 0.5f),
        buttonWidth,
        kSliderRowHeight
    };
}

static std::string formatSliderValue(float value)
{
    char buffer[32] = {};
    SDL_snprintf(buffer, sizeof(buffer), "%.2f", value);
    return std::string(buffer);
}

static int resolveWheelDelta(
    RC2D_MouseWheelDirection direction,
    float wheelY,
    Sint32 integerY)
{
    int delta = static_cast<int>(integerY);
    if (delta == 0)
    {
        if (wheelY > 0.0f)
        {
            delta = 1;
        }
        else if (wheelY < 0.0f)
        {
            delta = -1;
        }
    }

    if (delta == 0)
    {
        if (direction == RC2D_SCROLL_UP)
        {
            delta = 1;
        }
        else if (direction == RC2D_SCROLL_DOWN)
        {
            delta = -1;
        }
    }

    return delta;
}

static float previewDistanceForBandIndex(int distanceBandIndex)
{
    switch (std::clamp(distanceBandIndex, 0, kDistanceBandButtonCount - 1))
    {
        case 0:
            return 8.0f;
        case 1:
            return 15.0f;
        case 2:
            return 30.0f;
        default:
            return 48.0f;
    }
}

static float previewAngleForSectorIndex(int angleSectorIndex)
{
    switch (std::clamp(angleSectorIndex, 0, kAngleSectorButtonCount - 1))
    {
        case 0:
            return 34.5f;
        case 1:
            return 90.0f;
        case 2:
            return 145.5f;
        case 3:
            return 215.5f;
        case 4:
            return 270.5f;
        default:
            return 325.5f;
    }
}

static SDL_FPoint trajectoryScreenDirectionForAngle(float angleDeg)
{
    constexpr float kDegToRad = kPi / 180.0f;
    const float angleRad = angleDeg * kDegToRad;
    return SDL_FPoint{
        -std::cos(angleRad),
        -std::sin(angleRad),
    };
}

static float screenRadiusForTileDistanceAtAngle(
    const Map& map,
    float distanceTiles,
    float angleDeg)
{
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return 0.0f;
    }

    const SDL_FPoint dir = trajectoryScreenDirectionForAngle(angleDeg);
    const float tileDxPerPixel = ((dir.x / halfTileW) + (dir.y / halfTileH)) * 0.5f;
    const float tileDyPerPixel = ((dir.y / halfTileH) - (dir.x / halfTileW)) * 0.5f;
    const float tilesPerPixel =
        std::sqrt((tileDxPerPixel * tileDxPerPixel) + (tileDyPerPixel * tileDyPerPixel));
    if (tilesPerPixel <= 1.0e-6f)
    {
        return 0.0f;
    }

    return (std::max)(distanceTiles, 0.0f) / tilesPerPixel;
}

static SDL_FPoint trajectoryScreenPointAtTileDistance(
    const Map& map,
    const SDL_FPoint& centerScreen,
    float distanceTiles,
    float angleDeg)
{
    const SDL_FPoint dir = trajectoryScreenDirectionForAngle(angleDeg);
    const float radiusPixels = screenRadiusForTileDistanceAtAngle(map, distanceTiles, angleDeg);
    return SDL_FPoint{
        centerScreen.x + (dir.x * radiusPixels),
        centerScreen.y + (dir.y * radiusPixels),
    };
}

static void applyTrajectoryPreviewPlacement(int distanceBandIndex, int angleSectorIndex)
{
    if (gActiveCannonSalvoAttackerShip == nullptr || gActiveCannonSalvoTargetShip == nullptr)
    {
        return;
    }

    Ship& attackerShip = *gActiveCannonSalvoAttackerShip;
    Ship& targetShip = *gActiveCannonSalvoTargetShip;
    const SDL_FPoint attackerTile = attackerShip.getPositionTile();
    const float distanceTiles = previewDistanceForBandIndex(distanceBandIndex);
    const float angleDeg = previewAngleForSectorIndex(angleSectorIndex);
    const Map& map = GetCurrentMap();
    const SDL_FPoint attackerScreen = map.tileToScreenCenterFloat(attackerTile.x, attackerTile.y);
    const SDL_FPoint targetScreen =
        trajectoryScreenPointAtTileDistance(map, attackerScreen, distanceTiles, angleDeg);
    SDL_FPoint targetTile = map.screenToTile(targetScreen.x, targetScreen.y);
    targetTile.x =
        std::clamp(targetTile.x, 0.0f, static_cast<float>((std::max)(0, map.getWidthTiles() - 1)));
    targetTile.y =
        std::clamp(targetTile.y, 0.0f, static_cast<float>((std::max)(0, map.getHeightTiles() - 1)));
    targetShip.setPositionTile(targetTile.x, targetTile.y);
}

static unsigned int trajectoryDebugBit(int sliderIndex)
{
    if (sliderIndex < 0 || sliderIndex >= kTrajectorySliderCount)
    {
        return 0U;
    }
    return 1U << static_cast<unsigned int>(sliderIndex);
}

static bool isTrajectoryDebugEnabled(unsigned int mask, int sliderIndex)
{
    const unsigned int bit = trajectoryDebugBit(sliderIndex);
    return bit != 0U && (mask & bit) != 0U;
}

static void debugQuadBezierMidControl(
    float p0x,
    float p0y,
    float p2x,
    float p2y,
    float bowChordFraction,
    float arcSide,
    float* p1x,
    float* p1y)
{
    const float mx = (p0x + p2x) * 0.5f;
    const float my = (p0y + p2y) * 0.5f;
    const float bx = p2x - p0x;
    const float by = p2y - p0y;
    const float blen = std::sqrt(bx * bx + by * by);
    if (blen < 1.0e-4f)
    {
        *p1x = p0x;
        *p1y = p0y;
        return;
    }

    const float nAx = by / blen;
    const float nAy = -bx / blen;
    const float nBx = -by / blen;
    const float nBy = bx / blen;
    float nx = nAx;
    float ny = nAy;
    if (nBy < ny)
    {
        nx = nBx;
        ny = nBy;
    }

    const float side = (std::clamp)((std::isfinite(arcSide) ? arcSide : 0.0f), -1.0f, 1.0f);
    if (std::fabs(side) > 0.001f)
    {
        const float targetNx = (side < 0.0f) ? nAx : nBx;
        const float targetNy = (side < 0.0f) ? nAy : nBy;
        const float blend = std::fabs(side);
        nx = nx + ((targetNx - nx) * blend);
        ny = ny + ((targetNy - ny) * blend);
        const float nLen = std::sqrt(nx * nx + ny * ny);
        if (nLen > 1.0e-5f)
        {
            nx /= nLen;
            ny /= nLen;
        }
    }

    const float bow = bowChordFraction * blen;
    *p1x = mx + nx * bow;
    *p1y = my + ny * bow;
}

static void debugQuadBezierEval(
    float p0x,
    float p0y,
    float p1x,
    float p1y,
    float p2x,
    float p2y,
    float u,
    float* ox,
    float* oy)
{
    const float omt = 1.0f - u;
    *ox = omt * omt * p0x + 2.0f * omt * u * p1x + u * u * p2x;
    *oy = omt * omt * p0y + 2.0f * omt * u * p1y + u * u * p2y;
}

struct TrajectoryDebugGeometry
{
    SDL_FPoint attackerTile{};
    SDL_FPoint targetTile{};
    SDL_FPoint aimTile{};
    SDL_FPoint forwardStartTile{};
    SDL_FPoint startTile{};
    SDL_FPoint fanMinStartTile{};
    SDL_FPoint fanMaxStartTile{};
    SDL_FPoint noiseMinStartTile{};
    SDL_FPoint noiseMaxStartTile{};
    SDL_FPoint controlTile{};
    SDL_FPoint midpointTile{};
    SDL_FPoint dir{};
    SDL_FPoint perp{};
    float chordLen = 0.0f;
};

static bool buildTrajectoryDebugGeometry(
    const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning& tuning,
    TrajectoryDebugGeometry* geometry)
{
    if (geometry == nullptr)
    {
        return false;
    }

    SDL_FPoint attackerTile{};
    SDL_FPoint targetTile{};
    if (!tryGetTrajectoryPreviewShips(&attackerTile, &targetTile))
    {
        return false;
    }

    TrajectoryDebugGeometry out{};
    out.attackerTile = attackerTile;
    out.targetTile = targetTile;
    out.aimTile = out.targetTile;

    const float chordDx = out.aimTile.x - out.attackerTile.x;
    const float chordDy = out.aimTile.y - out.attackerTile.y;
    out.chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
    if (out.chordLen < 1.0e-5f)
    {
        return false;
    }

    const float invLen = 1.0f / out.chordLen;
    out.dir = SDL_FPoint{chordDx * invLen, chordDy * invLen};
    out.perp = SDL_FPoint{-chordDy * invLen, chordDx * invLen};
    const int arcSideSign = getArcSideSelectionSign(tuning.arcSide);
    const float sideOffset =
        static_cast<float>(arcSideSign) * std::fabs(tuning.launchSideOffsetTiles);
    out.forwardStartTile = SDL_FPoint{
        out.attackerTile.x + (out.dir.x * tuning.launchForwardOffsetTiles),
        out.attackerTile.y + (out.dir.y * tuning.launchForwardOffsetTiles)
    };
    out.startTile = SDL_FPoint{
        out.forwardStartTile.x + (out.perp.x * sideOffset),
        out.forwardStartTile.y + (out.perp.y * sideOffset)
    };
    out.fanMinStartTile = SDL_FPoint{
        out.startTile.x,
        out.startTile.y
    };
    out.fanMaxStartTile = SDL_FPoint{
        out.startTile.x + (out.perp.x * static_cast<float>(arcSideSign) * tuning.launchFanHalfTiles),
        out.startTile.y + (out.perp.y * static_cast<float>(arcSideSign) * tuning.launchFanHalfTiles)
    };
    out.noiseMinStartTile = SDL_FPoint{
        out.startTile.x,
        out.startTile.y
    };
    out.noiseMaxStartTile = SDL_FPoint{
        out.startTile.x +
            (out.perp.x * static_cast<float>(arcSideSign) * (tuning.launchFanHalfTiles + tuning.launchNoiseTiles)),
        out.startTile.y +
            (out.perp.y * static_cast<float>(arcSideSign) * (tuning.launchFanHalfTiles + tuning.launchNoiseTiles))
    };
    out.midpointTile = SDL_FPoint{
        (out.startTile.x + out.aimTile.x) * 0.5f,
        (out.startTile.y + out.aimTile.y) * 0.5f
    };

    debugQuadBezierMidControl(
        out.startTile.x,
        out.startTile.y,
        out.aimTile.x,
        out.aimTile.y,
        tuning.bezierBowChordFraction,
        tuning.arcSide,
        &out.controlTile.x,
        &out.controlTile.y);

    *geometry = out;
    return true;
}

static void drawDebugLine(const SDL_FPoint& a, const SDL_FPoint& b, RC2D_Color color)
{
    rc2d_graphics_setColor(color);
    rc2d_graphics_line(a.x, a.y, b.x, b.y);
}

static void drawDebugCross(const SDL_FPoint& p, float size, RC2D_Color color)
{
    rc2d_graphics_setColor(color);
    rc2d_graphics_line(p.x - size, p.y, p.x + size, p.y);
    rc2d_graphics_line(p.x, p.y - size, p.x, p.y + size);
}

static SDL_FPoint debugTileToScreen(const Map& map, const SDL_FPoint& tile)
{
    return map.tileToScreenCenterFloat(tile.x, tile.y);
}

static SDL_FPoint debugCurveSampleScreen(
    const Map& map,
    const TrajectoryDebugGeometry& geometry,
    const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning& tuning,
    float rawU,
    bool includeScreenLob,
    float laneOffsetTiles = 0.0f)
{
    const float u = remapDebugFastSlowFastProgress(rawU, tuning.flightEaseStrength);
    float tileX = 0.0f;
    float tileY = 0.0f;
    debugQuadBezierEval(
        geometry.startTile.x,
        geometry.startTile.y,
        geometry.controlTile.x,
        geometry.controlTile.y,
        geometry.aimTile.x,
        geometry.aimTile.y,
        u,
        &tileX,
        &tileY);
    if (std::fabs(laneOffsetTiles) > 0.001f)
    {
        const float chordDx = geometry.aimTile.x - geometry.startTile.x;
        const float chordDy = geometry.aimTile.y - geometry.startTile.y;
        const float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
        if (chordLen > 1.0e-5f)
        {
            const float laneCurve = std::sin(u * kPi);
            tileX += (-chordDy / chordLen) * laneOffsetTiles * laneCurve;
            tileY += (chordDx / chordLen) * laneOffsetTiles * laneCurve;
        }
    }

    SDL_FPoint screen = map.tileToScreenCenterFloat(tileX, tileY);
    if (includeScreenLob && tuning.screenLobPixels > 0.001f)
    {
        screen.y -= std::sin(u * kPi) * tuning.screenLobPixels * GetCamera().getZoomFactor();
    }
    return screen;
}

static void drawDebugCurve(
    const Map& map,
    const TrajectoryDebugGeometry& geometry,
    const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning& tuning,
    bool includeScreenLob,
    RC2D_Color color,
    float laneOffsetTiles = 0.0f)
{
    SDL_FPoint previous{};
    bool hasPrevious = false;
    for (int sampleIndex = 0; sampleIndex <= kDebugCurveSampleCount; ++sampleIndex)
    {
        const float u =
            static_cast<float>(sampleIndex) / static_cast<float>((std::max)(1, kDebugCurveSampleCount));
        const SDL_FPoint current =
            debugCurveSampleScreen(map, geometry, tuning, u, includeScreenLob, laneOffsetTiles);
        if (hasPrevious)
        {
            drawDebugLine(previous, current, color);
        }
        previous = current;
        hasPrevious = true;
    }
}

static void drawDebugLabel(
    RC2D_Font* font,
    const char* text,
    const SDL_FPoint& anchor,
    RC2D_Color color)
{
    drawTextAt(font, text, anchor.x + 8.0f, anchor.y - 8.0f, color);
}

static void drawTrajectoryAngleCompassOverlay(
    int selectedDistanceBandIndex,
    int selectedAngleSectorIndex,
    RC2D_Font* font)
{
    SDL_FPoint centerTile{};
    SDL_FPoint targetTileUnused{};
    if (!tryGetTrajectoryPreviewShips(&centerTile, &targetTileUnused))
    {
        return;
    }
    const Map& map = GetCurrentMap();
    const float distanceTiles = previewDistanceForBandIndex(selectedDistanceBandIndex);
    const int selectedSector = std::clamp(selectedAngleSectorIndex, 0, kAngleSectorButtonCount - 1);

    constexpr std::array<float, kAngleSectorButtonCount> sectorStartDeg = {{
        0.0f,
        70.0f,
        111.0f,
        181.0f,
        251.0f,
        291.0f,
    }};
    constexpr std::array<float, kAngleSectorButtonCount> sectorMidDeg = {{
        34.5f,
        90.0f,
        145.5f,
        215.5f,
        270.5f,
        325.5f,
    }};
    constexpr std::array<const char*, kAngleSectorButtonCount> sectorBoundaryLabels = {{
        "0",
        "70",
        "111",
        "181",
        "251",
        "291",
    }};

    constexpr RC2D_Color circleColor = RC2D_Color{230, 232, 220, 118};
    constexpr RC2D_Color boundaryColor = RC2D_Color{230, 232, 220, 150};
    constexpr RC2D_Color selectedColor = RC2D_Color{255, 190, 72, 228};
    constexpr RC2D_Color labelColor = RC2D_Color{246, 245, 235, 235};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    const SDL_FPoint centerScreen = debugTileToScreen(map, centerTile);
    SDL_FPoint previousScreen{};
    bool hasPrevious = false;
    constexpr int kCircleSampleCount = 96;
    for (int sampleIndex = 0; sampleIndex <= kCircleSampleCount; ++sampleIndex)
    {
        const float angleDeg =
            static_cast<float>(sampleIndex) * (360.0f / static_cast<float>(kCircleSampleCount));
        const SDL_FPoint screen =
            trajectoryScreenPointAtTileDistance(map, centerScreen, distanceTiles, angleDeg);
        if (hasPrevious)
        {
            drawDebugLine(previousScreen, screen, circleColor);
        }
        previousScreen = screen;
        hasPrevious = true;
    }

    for (int index = 0; index < kAngleSectorButtonCount; ++index)
    {
        const SDL_FPoint boundaryScreen = trajectoryScreenPointAtTileDistance(
            map,
            centerScreen,
            distanceTiles,
            sectorStartDeg[static_cast<size_t>(index)]);
        drawDebugLine(centerScreen, boundaryScreen, boundaryColor);
        drawDebugCross(boundaryScreen, kDebugMarkerSize - 1.0f, boundaryColor);
        drawDebugLabel(
            font,
            sectorBoundaryLabels[static_cast<size_t>(index)],
            boundaryScreen,
            labelColor);
    }

    const SDL_FPoint selectedScreen = trajectoryScreenPointAtTileDistance(
        map,
        centerScreen,
        distanceTiles,
        sectorMidDeg[static_cast<size_t>(selectedSector)]);
    drawDebugLine(centerScreen, selectedScreen, selectedColor);
    drawDebugCross(selectedScreen, kDebugMarkerSize + 1.0f, selectedColor);
    drawDebugLabel(
        font,
        kAngleSectorLabels[static_cast<size_t>(selectedSector)],
        selectedScreen,
        selectedColor);
}

static void drawTrajectoryDebugOverlay(
    unsigned int debugMask,
    int selectedDistanceBandIndex,
    int selectedAngleSectorIndex,
    RC2D_Font* font)
{
    if (debugMask == 0U)
    {
        return;
    }

    const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning tuning =
        MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
            selectedDistanceBandIndex,
            selectedAngleSectorIndex);

    TrajectoryDebugGeometry geometry{};
    if (!buildTrajectoryDebugGeometry(tuning, &geometry))
    {
        return;
    }

    const Map& map = GetCurrentMap();
    const SDL_FPoint attackerScreen = debugTileToScreen(map, geometry.attackerTile);
    const SDL_FPoint targetScreen = debugTileToScreen(map, geometry.targetTile);
    const SDL_FPoint startScreen = debugTileToScreen(map, geometry.startTile);
    const SDL_FPoint forwardStartScreen = debugTileToScreen(map, geometry.forwardStartTile);
    const SDL_FPoint controlScreen = debugTileToScreen(map, geometry.controlTile);
    const SDL_FPoint midpointScreen = debugTileToScreen(map, geometry.midpointTile);

    constexpr RC2D_Color chordColor = RC2D_Color{220, 224, 230, 130};
    constexpr RC2D_Color attackerColor = RC2D_Color{255, 154, 52, 245};
    constexpr RC2D_Color targetColor = RC2D_Color{255, 78, 78, 245};
    constexpr RC2D_Color baseCurveColor = RC2D_Color{84, 221, 255, 225};
    constexpr RC2D_Color lobCurveColor = RC2D_Color{255, 190, 72, 235};
    constexpr RC2D_Color controlColor = RC2D_Color{190, 140, 255, 230};
    constexpr RC2D_Color startColor = RC2D_Color{118, 255, 170, 230};
    constexpr RC2D_Color noiseColor = RC2D_Color{120, 176, 255, 210};
    constexpr RC2D_Color impactColor = RC2D_Color{255, 95, 95, 232};
    constexpr RC2D_Color timeColor = RC2D_Color{246, 245, 235, 220};
    constexpr RC2D_Color easeColor = RC2D_Color{255, 236, 120, 230};
    constexpr RC2D_Color laneColor = RC2D_Color{135, 255, 218, 210};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    drawDebugLine(attackerScreen, targetScreen, chordColor);
    drawDebugCross(attackerScreen, kDebugMarkerSize + 2.0f, attackerColor);
    drawDebugCross(targetScreen, kDebugMarkerSize + 2.0f, targetColor);
    drawDebugLabel(font, "ship", attackerScreen, attackerColor);
    drawDebugLabel(font, "target", targetScreen, targetColor);

    const bool showBezier = isTrajectoryDebugEnabled(debugMask, 0);
    const bool showArcSide = isTrajectoryDebugEnabled(debugMask, 1);
    const bool showLob = isTrajectoryDebugEnabled(debugMask, 2);
    const bool showStagger = isTrajectoryDebugEnabled(debugMask, 3);
    const bool showSideOut = isTrajectoryDebugEnabled(debugMask, 4);
    const bool showForwardOut = isTrajectoryDebugEnabled(debugMask, 5);
    const bool showFan = isTrajectoryDebugEnabled(debugMask, 6);
    const bool showNoise = isTrajectoryDebugEnabled(debugMask, 7);
    const bool showImpact = isTrajectoryDebugEnabled(debugMask, 8);
    const bool showLane = isTrajectoryDebugEnabled(debugMask, 9);
    const bool showDuration = isTrajectoryDebugEnabled(debugMask, 10);
    const bool showEase = isTrajectoryDebugEnabled(debugMask, 11);

    if (showBezier)
    {
        drawDebugCurve(map, geometry, tuning, false, baseCurveColor);
        drawDebugLine(startScreen, controlScreen, controlColor);
        drawDebugLine(controlScreen, targetScreen, controlColor);
        drawDebugCross(startScreen, kDebugMarkerSize, startColor);
        drawDebugCross(controlScreen, kDebugMarkerSize, controlColor);
        drawDebugLabel(font, "P0", startScreen, startColor);
        drawDebugLabel(font, "P1", controlScreen, controlColor);
    }

    if (showArcSide)
    {
        drawDebugLine(midpointScreen, controlScreen, controlColor);
        drawDebugCross(midpointScreen, kDebugMarkerSize, controlColor);
        drawDebugLabel(font, "arc side", controlScreen, controlColor);
    }

    if (showLob)
    {
        drawDebugCurve(map, geometry, tuning, true, lobCurveColor);
        const SDL_FPoint baseMid = debugCurveSampleScreen(map, geometry, tuning, 0.5f, false);
        const SDL_FPoint lobMid = debugCurveSampleScreen(map, geometry, tuning, 0.5f, true);
        drawDebugLine(baseMid, lobMid, lobCurveColor);
        drawDebugCross(lobMid, kDebugMarkerSize, lobCurveColor);
        drawDebugLabel(font, "lob", lobMid, lobCurveColor);
    }

    if (showStagger)
    {
        const float gapPx = 8.0f + (18.0f * std::clamp(tuning.launchStaggerScale, 0.05f, 2.5f));
        SDL_FPoint tickBase = startScreen;
        tickBase.x += 12.0f;
        tickBase.y += 24.0f;
        for (int tick = 0; tick < 5; ++tick)
        {
            const float x = tickBase.x + (static_cast<float>(tick) * gapPx);
            drawDebugLine(
                SDL_FPoint{x, tickBase.y - 5.0f},
                SDL_FPoint{x, tickBase.y + 5.0f},
                timeColor);
        }
        drawDebugLabel(font, "salvo gap", tickBase, timeColor);
    }

    if (showForwardOut)
    {
        drawDebugLine(attackerScreen, forwardStartScreen, startColor);
        drawDebugCross(forwardStartScreen, kDebugMarkerSize, startColor);
        drawDebugLabel(font, "forward", forwardStartScreen, startColor);
    }

    if (showSideOut)
    {
        drawDebugLine(forwardStartScreen, startScreen, startColor);
        drawDebugCross(startScreen, kDebugMarkerSize, startColor);
        drawDebugLabel(font, "side out", startScreen, startColor);
    }

    if (showFan)
    {
        const SDL_FPoint fanMinScreen = debugTileToScreen(map, geometry.fanMinStartTile);
        const SDL_FPoint fanMaxScreen = debugTileToScreen(map, geometry.fanMaxStartTile);
        drawDebugLine(fanMinScreen, fanMaxScreen, startColor);
        drawDebugCross(fanMinScreen, kDebugMarkerSize, startColor);
        drawDebugCross(fanMaxScreen, kDebugMarkerSize, startColor);
        drawDebugLabel(font, "fan", fanMaxScreen, startColor);
    }

    if (showNoise)
    {
        const SDL_FPoint noiseMinScreen = debugTileToScreen(map, geometry.noiseMinStartTile);
        const SDL_FPoint noiseMaxScreen = debugTileToScreen(map, geometry.noiseMaxStartTile);
        drawDebugLine(noiseMinScreen, noiseMaxScreen, noiseColor);
        drawDebugCross(noiseMinScreen, kDebugMarkerSize, noiseColor);
        drawDebugCross(noiseMaxScreen, kDebugMarkerSize, noiseColor);
        drawDebugLabel(font, "noise range", noiseMaxScreen, noiseColor);
    }

    if (showImpact)
    {
        constexpr std::array<int, 9> impactDx = {{
            0, -1, -1, -1, 0, 0, 1, 1, 1,
        }};
        constexpr std::array<int, 9> impactDy = {{
            0, -1, 0, 1, -1, 1, -1, 0, 1,
        }};
        for (size_t index = 0; index < impactDx.size(); ++index)
        {
            float impactOffX = static_cast<float>(impactDx[index]) * tuning.impactSpreadScale;
            float impactOffY = static_cast<float>(impactDy[index]) * tuning.impactSpreadScale;
            forceDebugOffsetToArcSide(
                geometry.targetTile.x - geometry.attackerTile.x,
                geometry.targetTile.y - geometry.attackerTile.y,
                getArcSideSelectionSign(tuning.arcSide),
                &impactOffX,
                &impactOffY);
            const SDL_FPoint impactTile = SDL_FPoint{
                geometry.targetTile.x + impactOffX,
                geometry.targetTile.y + impactOffY
            };
            const SDL_FPoint impactScreen = debugTileToScreen(map, impactTile);
            drawDebugCross(impactScreen, kDebugMarkerSize, impactColor);
        }
        drawDebugLabel(font, "impact spread", targetScreen, impactColor);
    }

    if (showLane)
    {
        const float sideSign = static_cast<float>(getArcSideSelectionSign(tuning.arcSide));
        const float laneMax = sideSign * tuning.flightLaneJitterTiles;
        if (std::fabs(laneMax) > 0.001f)
        {
            drawDebugCurve(map, geometry, tuning, showLob, laneColor, laneMax * 0.35f);
            drawDebugCurve(map, geometry, tuning, showLob, laneColor, laneMax * 0.70f);
            drawDebugCurve(map, geometry, tuning, showLob, laneColor, laneMax);
            const SDL_FPoint laneMid =
                debugCurveSampleScreen(map, geometry, tuning, 0.5f, showLob, laneMax);
            drawDebugCross(laneMid, kDebugMarkerSize, laneColor);
            drawDebugLabel(font, "relache file", laneMid, laneColor);
        }
        else
        {
            drawDebugLabel(font, "relache file 0", midpointScreen, laneColor);
        }
    }

    if (showDuration)
    {
        const bool useLobForSamples = showLob;
        for (int sampleIndex = 1; sampleIndex <= 4; ++sampleIndex)
        {
            const float u = static_cast<float>(sampleIndex) * 0.25f;
            const SDL_FPoint p = debugCurveSampleScreen(map, geometry, tuning, u, useLobForSamples);
            drawDebugCross(p, kDebugMarkerSize - 1.0f, timeColor);
        }

        char buffer[32] = {};
        SDL_snprintf(buffer, sizeof(buffer), "duration x%.2f", tuning.flightDurationScale);
        drawDebugLabel(font, buffer, debugCurveSampleScreen(map, geometry, tuning, 0.75f, useLobForSamples), timeColor);
    }

    if (showEase)
    {
        const bool useLobForSamples = showLob;
        for (int sampleIndex = 1; sampleIndex <= 7; ++sampleIndex)
        {
            const float rawU = static_cast<float>(sampleIndex) / 8.0f;
            const SDL_FPoint p = debugCurveSampleScreen(map, geometry, tuning, rawU, useLobForSamples);
            drawDebugCross(p, kDebugMarkerSize - 1.0f, easeColor);
        }

        char buffer[32] = {};
        SDL_snprintf(buffer, sizeof(buffer), "rythme x%.2f", tuning.flightEaseStrength);
        drawDebugLabel(font, buffer, debugCurveSampleScreen(map, geometry, tuning, 0.5f, useLobForSamples), easeColor);
    }
}

struct TrailPreviewPoint
{
    SDL_FPoint screen{};
    float distanceFromHeadTiles = 0.0f;
    float widthPixels = 0.0f;
    RC2D_Color color{};
};

struct TrailPreviewSample
{
    SDL_FPoint position{};
    SDL_FPoint tangent{1.0f, 0.0f};
    float trailT = 0.0f;
    float widthRatio = 1.0f;
    float opacity = 1.0f;
    std::uint8_t tint[3]{255, 255, 255};
};

static std::vector<TrailPreviewPoint> buildTrailPreviewPoints(
    const SDL_FRect& viewport,
    const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig& config,
    float animTimeSec,
    bool fullLength)
{
    std::vector<TrailPreviewPoint> points;
    if (viewport.w <= 8.0f || viewport.h <= 8.0f)
    {
        return points;
    }

    const float clampedMaxLength = (std::max)(config.maxLengthTiles, 0.15f);
    const float animWave = 0.5f + (0.5f * std::sin(animTimeSec * 1.25f));
    const float visibleLengthTiles =
        fullLength
            ? clampedMaxLength
            : std::clamp(clampedMaxLength * (0.34f + (0.66f * animWave)), 0.15f, clampedMaxLength);
    const float headX = viewport.x + (viewport.w * 0.82f);
    const float headY = viewport.y + (viewport.h * 0.55f) +
        (fullLength ? 0.0f : (std::sin(animTimeSec * 1.8f) * viewport.h * 0.03f));
    constexpr int sampleCount = 42;
    points.reserve(static_cast<std::size_t>(sampleCount));
    for (int index = 0; index < sampleCount; ++index)
    {
        const float t =
            (sampleCount > 1) ? (static_cast<float>(index) / static_cast<float>(sampleCount - 1)) : 0.0f;
        const float distanceFromHeadTiles = visibleLengthTiles * t;
        const float x = headX - (viewport.w * 0.72f * t);
        const float baseCurve = std::sin(t * kPi) * viewport.h * (fullLength ? 0.12f : 0.16f);
        const float waveCurve =
            fullLength
                ? 0.0f
                : (std::sin((t * 4.0f) + (animTimeSec * 1.6f)) * viewport.h * 0.02f);
        const float y = headY - (baseCurve + waveCurve) + (t * t * viewport.h * 0.05f);

        const float trailT =
            (clampedMaxLength > 1.0e-5f)
                ? std::clamp(distanceFromHeadTiles / clampedMaxLength, 0.0f, 1.0f)
                : 1.0f;
        const float widthT = std::pow(trailT, config.widthExponent);
        const float alphaT = std::pow(trailT, config.opacityExponent);
        const float widthPixels =
            config.headWidthPixels + ((config.tailWidthPixels - config.headWidthPixels) * widthT);
        const float opacity =
            config.headOpacity + ((config.tailOpacity - config.headOpacity) * alphaT);
        const auto mixChannel = [&](float headChannel, float tailChannel) -> std::uint8_t {
            const float mixed = std::clamp(headChannel + ((tailChannel - headChannel) * trailT), 0.0f, 255.0f);
            return static_cast<std::uint8_t>(std::lround(mixed));
        };

        TrailPreviewPoint point{};
        point.screen = SDL_FPoint{x, y};
        point.distanceFromHeadTiles = distanceFromHeadTiles;
        point.widthPixels = (std::max)(1.0f, widthPixels * 0.56f);
        point.color = RC2D_Color{
            mixChannel(config.headColorR, config.tailColorR),
            mixChannel(config.headColorG, config.tailColorG),
            mixChannel(config.headColorB, config.tailColorB),
            static_cast<std::uint8_t>(std::lround(std::clamp(opacity * 255.0f, 0.0f, 255.0f)))
        };
        points.push_back(point);
    }

    if (config.headCoverTiles > 1.0e-4f && points.size() >= 2U)
    {
        const float screenDx = points[0U].screen.x - points[1U].screen.x;
        const float screenDy = points[0U].screen.y - points[1U].screen.y;
        const float screenLen = std::sqrt(screenDx * screenDx + screenDy * screenDy);
        const float tileDx = points[0U].distanceFromHeadTiles - points[1U].distanceFromHeadTiles;
        const float tileLen = std::fabs(tileDx);
        if (screenLen > 1.0e-5f && tileLen > 1.0e-5f)
        {
            const float forwardX = screenDx / screenLen;
            const float forwardY = screenDy / screenLen;
            const float pixelsPerTile = screenLen / tileLen;
            TrailPreviewPoint coverPoint = points[0U];
            coverPoint.screen.x += forwardX * config.headCoverTiles * pixelsPerTile;
            coverPoint.screen.y += forwardY * config.headCoverTiles * pixelsPerTile;
            coverPoint.distanceFromHeadTiles = -config.headCoverTiles;
            points.insert(points.begin(), coverPoint);
        }
    }

    return points;
}

static bool sampleTrailPreviewAtDistance(
    const std::vector<TrailPreviewPoint>& points,
    const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig& config,
    float sampleDist,
    TrailPreviewSample* outSample)
{
    if (outSample == nullptr || points.size() < 2U)
    {
        return false;
    }

    std::size_t segmentIndex = 0U;
    while (segmentIndex + 1U < points.size() &&
           points[segmentIndex + 1U].distanceFromHeadTiles < sampleDist)
    {
        ++segmentIndex;
    }
    if (segmentIndex + 1U >= points.size())
    {
        return false;
    }

    const TrailPreviewPoint& a = points[segmentIndex];
    const TrailPreviewPoint& b = points[segmentIndex + 1U];
    const float segSpan = b.distanceFromHeadTiles - a.distanceFromHeadTiles;
    const float segT =
        (segSpan > 1.0e-5f)
            ? std::clamp((sampleDist - a.distanceFromHeadTiles) / segSpan, 0.0f, 1.0f)
            : 0.0f;
    const float posX = a.screen.x + ((b.screen.x - a.screen.x) * segT);
    const float posY = a.screen.y + ((b.screen.y - a.screen.y) * segT);
    SDL_FPoint tangent{b.screen.x - a.screen.x, b.screen.y - a.screen.y};
    const float tangentLen = std::sqrt((tangent.x * tangent.x) + (tangent.y * tangent.y));
    if (tangentLen > 1.0e-5f)
    {
        tangent.x /= tangentLen;
        tangent.y /= tangentLen;
    }
    else
    {
        tangent = SDL_FPoint{1.0f, 0.0f};
    }

    const float maxLength = (std::max)(config.maxLengthTiles, 0.15f);
    const float trailT =
        (maxLength > 1.0e-5f)
            ? std::clamp(sampleDist / maxLength, 0.0f, 1.0f)
            : 1.0f;
    const float widthT = std::pow(trailT, config.widthExponent);
    const float widthRatio =
        (config.headWidthPixels > 1.0e-5f)
            ? ((config.headWidthPixels + ((config.tailWidthPixels - config.headWidthPixels) * widthT)) /
                config.headWidthPixels)
            : 1.0f;
    const float opacity =
        config.stampHeadOpacity + ((config.stampTailOpacity - config.stampHeadOpacity) * trailT);
    const auto mixTrailChannel = [&](float headChannel, float tailChannel) -> std::uint8_t {
        const float mixed = std::clamp(headChannel + ((tailChannel - headChannel) * trailT), 0.0f, 255.0f);
        return static_cast<std::uint8_t>(std::lround(mixed));
    };
    const std::uint8_t trailR = mixTrailChannel(config.headColorR, config.tailColorR);
    const std::uint8_t trailG = mixTrailChannel(config.headColorG, config.tailColorG);
    const std::uint8_t trailB = mixTrailChannel(config.headColorB, config.tailColorB);
    const auto mixTint = [&](std::uint8_t trailChannel) -> std::uint8_t {
        const float mixed = 255.0f + ((static_cast<float>(trailChannel) - 255.0f) * config.stampTintStrength);
        return static_cast<std::uint8_t>(std::lround(std::clamp(mixed, 0.0f, 255.0f)));
    };

    outSample->position = SDL_FPoint{posX, posY};
    outSample->tangent = tangent;
    outSample->trailT = trailT;
    outSample->widthRatio = widthRatio;
    outSample->opacity = opacity;
    outSample->tint[0] = mixTint(trailR);
    outSample->tint[1] = mixTint(trailG);
    outSample->tint[2] = mixTint(trailB);
    return true;
}

static void drawTrailPreviewPanel(
    const SDL_FRect& rect,
    const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig& config,
    const VFXClassic* projectileVfx,
    const VFXClassic* trailVfx,
    bool projectileIlluminated,
    bool trailEnabled,
    float animTimeSec,
    RC2D_Font* font,
    const std::string& projectileLabel,
    const std::string& trailLabel,
    const SDL_FRect* manualRect,
    int selectedManualStampIndex)
{
    fillAndOutlineRect(rect, kPreviewFill, kPreviewBorder);

    drawTextAt(font, "Preview live", rect.x + 12.0f, rect.y + 10.0f, kHelpAccent);
    drawTextAt(
        font,
        trailEnabled ? "Boulet + ribbon trail en direct" : "Boulet seul : trail desactive",
        rect.x + 12.0f,
        rect.y + 28.0f,
        kTextMuted);

    const SDL_FRect viewport = SDL_FRect{
        rect.x + 10.0f,
        rect.y + 50.0f,
        rect.w - 20.0f,
        rect.h - 84.0f
    };
    if (viewport.w <= 8.0f || viewport.h <= 8.0f)
    {
        return;
    }

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr)
    {
        const SDL_Rect clipRect = toClipRect(viewport);
        SDL_SetRenderClipRect(renderer, &clipRect);
    }

    for (int gridIndex = 1; gridIndex < 4; ++gridIndex)
    {
        const float y = viewport.y + (viewport.h * (static_cast<float>(gridIndex) / 4.0f));
        rc2d_graphics_setColor(kPreviewGrid);
        rc2d_graphics_line(viewport.x, y, viewport.x + viewport.w, y);
    }
    for (int gridIndex = 1; gridIndex < 5; ++gridIndex)
    {
        const float x = viewport.x + (viewport.w * (static_cast<float>(gridIndex) / 5.0f));
        rc2d_graphics_setColor(kPreviewGrid);
        rc2d_graphics_line(x, viewport.y, x, viewport.y + viewport.h);
    }

    const float simulatedShotDistanceTiles =
        2.0f + (18.0f * (0.5f + (0.5f * std::sin(animTimeSec * 0.8f))));
    const bool trailAllowedByShotDistance =
        config.hideNearTargetTiles <= 1.0e-4f ||
        simulatedShotDistanceTiles > config.hideNearTargetTiles;
    std::vector<TrailPreviewPoint> points = buildTrailPreviewPoints(viewport, config, animTimeSec, false);
    const float headX = viewport.x + (viewport.w * 0.82f);
    const float headY = viewport.y + (viewport.h * 0.55f) +
        (std::sin(animTimeSec * 1.8f) * viewport.h * 0.03f);

    if (trailEnabled && trailAllowedByShotDistance && config.meshEnabled && points.size() >= 2U)
    {
        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
        vertices.reserve(points.size() * 2U);
        indices.reserve((points.size() - 1U) * 6U);
        for (std::size_t index = 0U; index < points.size(); ++index)
        {
            SDL_FPoint tangent{};
            if (index == 0U)
            {
                tangent.x = points[1U].screen.x - points[0U].screen.x;
                tangent.y = points[1U].screen.y - points[0U].screen.y;
            }
            else if (index + 1U >= points.size())
            {
                tangent.x = points[index].screen.x - points[index - 1U].screen.x;
                tangent.y = points[index].screen.y - points[index - 1U].screen.y;
            }
            else
            {
                tangent.x = points[index + 1U].screen.x - points[index - 1U].screen.x;
                tangent.y = points[index + 1U].screen.y - points[index - 1U].screen.y;
            }

            const float tangentLen = std::sqrt((tangent.x * tangent.x) + (tangent.y * tangent.y));
            if (tangentLen > 1.0e-5f)
            {
                tangent.x /= tangentLen;
                tangent.y /= tangentLen;
            }
            else
            {
                tangent = SDL_FPoint{1.0f, 0.0f};
            }

            const SDL_FPoint normal{-tangent.y, tangent.x};
            const float halfWidth = points[index].widthPixels * 0.5f;
            SDL_Vertex left{};
            left.position.x = points[index].screen.x + (normal.x * halfWidth);
            left.position.y = points[index].screen.y + (normal.y * halfWidth);
            left.color = SDL_FColor{
                static_cast<float>(points[index].color.r) / 255.0f,
                static_cast<float>(points[index].color.g) / 255.0f,
                static_cast<float>(points[index].color.b) / 255.0f,
                static_cast<float>(points[index].color.a) / 255.0f
            };
            left.tex_coord = SDL_FPoint{0.0f, 0.0f};

            SDL_Vertex right = left;
            right.position.x = points[index].screen.x - (normal.x * halfWidth);
            right.position.y = points[index].screen.y - (normal.y * halfWidth);
            right.tex_coord = SDL_FPoint{1.0f, 0.0f};

            vertices.push_back(left);
            vertices.push_back(right);

            if (index > 0U)
            {
                const int base = static_cast<int>((index - 1U) * 2U);
                indices.push_back(base + 0);
                indices.push_back(base + 1);
                indices.push_back(base + 2);
                indices.push_back(base + 1);
                indices.push_back(base + 3);
                indices.push_back(base + 2);
            }
        }

        if (!vertices.empty() && !indices.empty())
        {
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
            (void)rc2d_graphics_renderGeometry(
                nullptr,
                vertices.data(),
                static_cast<int>(vertices.size()),
                indices.data(),
                static_cast<int>(indices.size()));
            rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        }
    }

    if (trailEnabled &&
        trailAllowedByShotDistance &&
        config.stampsEnabled &&
        trailVfx != nullptr &&
        trailVfx->isLoaded() &&
        !trailVfx->isFinished() &&
        points.size() >= 2U)
    {
        auto drawPreviewStampAt = [&](float sampleDist, int stampIndex, const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig::CustomStampPlacement* customStamp) {
            TrailPreviewSample sample{};
            if (!sampleTrailPreviewAtDistance(points, config, sampleDist, &sample))
            {
                return;
            }

            const SDL_FPoint normal{-sample.tangent.y, sample.tangent.x};
            float posX = sample.position.x;
            float posY = sample.position.y;
            float rotDeg = std::atan2(sample.tangent.y, sample.tangent.x) * (180.0f / kPi);
            float scaleMul = 1.0f;
            float opacityMul = 1.0f;
            if (customStamp != nullptr)
            {
                posX += normal.x * customStamp->lateralOffsetPixels;
                posY += normal.y * customStamp->lateralOffsetPixels;
                rotDeg += customStamp->rotationOffsetDeg;
                scaleMul = customStamp->scale;
                opacityMul = customStamp->opacity;
            }

            const std::uint8_t alpha = static_cast<std::uint8_t>(
                std::lround(std::clamp(sample.opacity * opacityMul * 255.0f, 0.0f, 255.0f)));
            const float phaseOffsetSec =
                config.stampPhaseOffsetSeconds * static_cast<float>(stampIndex);
            trailVfx->drawWithTintAlphaBlendPhaseOffset(
                posX,
                posY,
                config.stampScale * scaleMul * (std::max)(0.20f, sample.widthRatio) * 0.74f,
                rotDeg,
                false,
                false,
                sample.tint,
                alpha,
                config.additiveStampBlend ? static_cast<int>(SDL_BLENDMODE_ADD) : 0,
                phaseOffsetSec);
        };

        if (config.manualStampsEnabled && !config.customStamps.empty())
        {
            for (std::size_t index = 0U; index < config.customStamps.size(); ++index)
            {
                const auto& stamp = config.customStamps[index];
                drawPreviewStampAt(stamp.distanceFromHeadTiles, static_cast<int>(index), &stamp);
            }
        }
        else
        {
            const float spacing = (std::max)(config.stampSpacingTiles, 0.05f);
            const float startDistance =
                (std::max)(-config.headCoverTiles, (std::min)(spacing * 0.45f, points.back().distanceFromHeadTiles));
            int stampIndex = 0;
            for (float sampleDist = startDistance;
                 sampleDist <= points.back().distanceFromHeadTiles + 0.0001f;
                 sampleDist += spacing, ++stampIndex)
            {
                drawPreviewStampAt(sampleDist, stampIndex, nullptr);
            }
        }
    }

    if (projectileVfx != nullptr && projectileVfx->isLoaded() && !projectileVfx->isFinished())
    {
        const std::uint8_t headR = static_cast<std::uint8_t>(
            std::lround(std::clamp(config.headColorR, 0.0f, 255.0f)));
        const std::uint8_t headG = static_cast<std::uint8_t>(
            std::lround(std::clamp(config.headColorG, 0.0f, 255.0f)));
        const std::uint8_t headB = static_cast<std::uint8_t>(
            std::lround(std::clamp(config.headColorB, 0.0f, 255.0f)));
        if (projectileIlluminated)
        {
            projectileVfx->drawIlluminatedProjectile(
                headX,
                headY,
                0.90f,
                0.0f,
                false,
                false,
                headR,
                headG,
                headB,
                nullptr,
                true);
        }
        else
        {
            projectileVfx->draw(headX, headY, 0.90f, 0.0f, false, false);
        }
    }

    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }

    drawTextAt(
        font,
        ("Boulet: " + (projectileLabel.empty() ? std::string("aucun") : projectileLabel)).c_str(),
        rect.x + 12.0f,
        rect.y + rect.h - 28.0f,
        kTextPrimary);
    drawTextAt(
        font,
        ("Trail: " + (trailLabel.empty() ? std::string("aucun") : trailLabel)).c_str(),
        rect.x + 12.0f,
        rect.y + rect.h - 12.0f,
        trailEnabled ? kTextMuted : kHelpAccent);
    if (config.hideNearTargetTiles > 1.0e-4f)
    {
        char thresholdBuffer[96] = {};
        SDL_snprintf(
            thresholdBuffer,
            sizeof(thresholdBuffer),
            "Distance tir simulee %.1f tuiles | seuil %.1f",
            simulatedShotDistanceTiles,
            config.hideNearTargetTiles);
        drawTextAt(
            font,
            thresholdBuffer,
            rect.x + 12.0f,
            rect.y + rect.h - 44.0f,
            trailAllowedByShotDistance ? kTextMuted : kHelpAccent);
    }

    if (manualRect != nullptr)
    {
        fillAndOutlineRect(*manualRect, kPreviewFill, kPreviewBorder);
        drawTextAt(font, "Preview placement manuel", manualRect->x + 12.0f, manualRect->y + 10.0f, kHelpAccent);
        drawTextAt(
            font,
            config.manualStampsEnabled ? "Longueur max + positions custom actives" : "Longueur max pour poser / regler les stamps",
            manualRect->x + 12.0f,
            manualRect->y + 28.0f,
            kTextMuted);

        const SDL_FRect manualViewport = SDL_FRect{
            manualRect->x + 10.0f,
            manualRect->y + 50.0f,
            manualRect->w - 20.0f,
            manualRect->h - 60.0f
        };
        if (manualViewport.w > 8.0f && manualViewport.h > 8.0f)
        {
            if (renderer != nullptr)
            {
                const SDL_Rect clipRect = toClipRect(manualViewport);
                SDL_SetRenderClipRect(renderer, &clipRect);
            }

            for (int gridIndex = 1; gridIndex < 5; ++gridIndex)
            {
                const float x = manualViewport.x + (manualViewport.w * (static_cast<float>(gridIndex) / 5.0f));
                rc2d_graphics_setColor(kPreviewGrid);
                rc2d_graphics_line(x, manualViewport.y, x, manualViewport.y + manualViewport.h);
            }
            const float centerY = manualViewport.y + (manualViewport.h * 0.5f);
            rc2d_graphics_setColor(kPreviewGrid);
            rc2d_graphics_line(manualViewport.x, centerY, manualViewport.x + manualViewport.w, centerY);

            const std::vector<TrailPreviewPoint> manualPoints =
                buildTrailPreviewPoints(manualViewport, config, animTimeSec, true);
            if (config.meshEnabled && manualPoints.size() >= 2U)
            {
                for (std::size_t index = 1U; index < manualPoints.size(); ++index)
                {
                    rc2d_graphics_setColor(manualPoints[index].color);
                    rc2d_graphics_line(
                        manualPoints[index - 1U].screen.x,
                        manualPoints[index - 1U].screen.y,
                        manualPoints[index].screen.x,
                        manualPoints[index].screen.y);
                }
            }

            if (trailVfx != nullptr && trailVfx->isLoaded() && !trailVfx->isFinished())
            {
                for (std::size_t index = 0U; index < config.customStamps.size(); ++index)
                {
                    const auto& stamp = config.customStamps[index];
                    TrailPreviewSample sample{};
                    if (!sampleTrailPreviewAtDistance(manualPoints, config, stamp.distanceFromHeadTiles, &sample))
                    {
                        continue;
                    }

                    const SDL_FPoint normal{-sample.tangent.y, sample.tangent.x};
                    const float posX = sample.position.x + (normal.x * stamp.lateralOffsetPixels);
                    const float posY = sample.position.y + (normal.y * stamp.lateralOffsetPixels);
                    const float rotDeg =
                        (std::atan2(sample.tangent.y, sample.tangent.x) * (180.0f / kPi)) + stamp.rotationOffsetDeg;
                    const std::uint8_t alpha = static_cast<std::uint8_t>(
                        std::lround(std::clamp(sample.opacity * stamp.opacity * 255.0f, 0.0f, 255.0f)));
                    trailVfx->drawWithTintAlphaBlendPhaseOffset(
                        posX,
                        posY,
                        config.stampScale * stamp.scale * (std::max)(0.20f, sample.widthRatio) * 0.74f,
                        rotDeg,
                        false,
                        false,
                        sample.tint,
                        alpha,
                        config.additiveStampBlend ? static_cast<int>(SDL_BLENDMODE_ADD) : 0,
                        config.stampPhaseOffsetSeconds * static_cast<float>(index));

                    const RC2D_Color markerColor =
                        (static_cast<int>(index) == selectedManualStampIndex)
                            ? kTextStrong
                            : RC2D_Color{160, 196, 235, 220};
                    rc2d_graphics_setColor(markerColor);
                    const SDL_FRect markerRect{posX - 4.0f, posY - 4.0f, 8.0f, 8.0f};
                    rc2d_graphics_rectangle("line", &markerRect);
                }
            }

            if (projectileVfx != nullptr && projectileVfx->isLoaded() && !projectileVfx->isFinished())
            {
                const float manualHeadX = manualViewport.x + (manualViewport.w * 0.82f);
                const float manualHeadY = manualViewport.y + (manualViewport.h * 0.55f);
                if (projectileIlluminated)
                {
                    projectileVfx->drawIlluminatedProjectile(
                        manualHeadX,
                        manualHeadY,
                        0.90f,
                        0.0f,
                        false,
                        false,
                        static_cast<std::uint8_t>(std::lround(std::clamp(config.headColorR, 0.0f, 255.0f))),
                        static_cast<std::uint8_t>(std::lround(std::clamp(config.headColorG, 0.0f, 255.0f))),
                        static_cast<std::uint8_t>(std::lround(std::clamp(config.headColorB, 0.0f, 255.0f))),
                        nullptr,
                        true);
                }
                else
                {
                    projectileVfx->draw(manualHeadX, manualHeadY, 0.90f, 0.0f, false, false);
                }
            }

            if (renderer != nullptr)
            {
                SDL_SetRenderClipRect(renderer, nullptr);
            }
        }
    }
}
} // namespace

IlluminatedProjectileDebugPanel::IlluminatedProjectileDebugPanel(void)
    : loaded(false),
      visible(true),
      panelDragging(false),
      sliderDragging(false),
      activeSliderIndex(-1),
      activePageIndex(0),
      selectedDistanceBandIndex(0),
      selectedAngleSectorIndex(0),
      trajectoryDebugMask(0U),
      trajectoryCircleVisible(true),
      trailScrollDragging(false),
      sliderDragGrabOffsetX(0.0f),
      panelDragGrabOffsetX(0.0f),
      panelDragGrabOffsetY(0.0f),
      trailScrollOffsetY(0.0f),
      trailScrollDragGrabOffsetY(0.0f),
      statusMessageTimerSec(0.0f),
      panelOffset{0.0f, 0.0f},
      statusMessage{},
      titleFont{},
      bodyFont{},
      previewProjectileVfx{},
      previewTrailVfx{},
      previewProjectileFolderPath{},
      previewTrailFolderPath{},
      loadedPreviewProjectileFolderPath{},
      loadedPreviewTrailFolderPath{},
      previewProjectileIlluminated(false),
      previewTrailEnabled(false),
      previewAssetsDirty(true),
      previewAnimTimerSec(0.0f),
      selectedManualTrailStampIndex(-1),
      manualTrailStampDragActive(false),
      pendingDialogRequest(DialogRequest::NONE)
{
}

IlluminatedProjectileDebugPanel::~IlluminatedProjectileDebugPanel(void)
{
}

void IlluminatedProjectileDebugPanel::load(void)
{
    (void)VFXClassic::loadIlluminatedProjectileGlowConfigFromFile();
    this->titleFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Semibold.ttf",
        RC2D_STORAGE_TITLE,
        15.0f);
    this->bodyFont = OpenStorageFont(
        "assets/fonts/SegoeUI-Regular.ttf",
        RC2D_STORAGE_TITLE,
        13.0f);

    this->loaded = true;
    this->visible = true;
    this->panelDragging = false;
    this->sliderDragging = false;
    this->trailScrollDragging = false;
    this->activeSliderIndex = -1;
    this->activePageIndex = 0;
    this->selectedDistanceBandIndex = 0;
    this->selectedAngleSectorIndex = 0;
    this->trajectoryDebugMask = 0U;
    this->trajectoryCircleVisible = true;
    this->trailScrollDragging = false;
    this->sliderDragGrabOffsetX = 0.0f;
    this->panelDragGrabOffsetX = 0.0f;
    this->panelDragGrabOffsetY = 0.0f;
    this->trailScrollOffsetY = 0.0f;
    this->trailScrollDragGrabOffsetY = 0.0f;
    this->statusMessageTimerSec = 0.0f;
    this->statusMessage.clear();
    this->panelOffset = SDL_FPoint{0.0f, 0.0f};
    this->previewProjectileVfx.unload();
    this->previewTrailVfx.unload();
    this->loadedPreviewProjectileFolderPath.clear();
    this->loadedPreviewTrailFolderPath.clear();
    this->previewAssetsDirty = true;
    this->previewAnimTimerSec = 0.0f;
    this->selectedManualTrailStampIndex = -1;
    this->manualTrailStampDragActive = false;
    this->pendingDialogRequest = DialogRequest::NONE;
}

void IlluminatedProjectileDebugPanel::unload(void)
{
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
    this->previewProjectileVfx.unload();
    this->previewTrailVfx.unload();
    this->loaded = false;
    this->panelDragging = false;
    this->sliderDragging = false;
    this->trailScrollDragging = false;
    this->activeSliderIndex = -1;
    this->selectedManualTrailStampIndex = -1;
    this->manualTrailStampDragActive = false;
    this->trajectoryDebugMask = 0U;
    this->trailScrollOffsetY = 0.0f;
    this->trailScrollDragGrabOffsetY = 0.0f;
    this->statusMessageTimerSec = 0.0f;
    this->statusMessage.clear();
    this->loadedPreviewProjectileFolderPath.clear();
    this->loadedPreviewTrailFolderPath.clear();
    this->previewAssetsDirty = true;
    this->previewAnimTimerSec = 0.0f;
    this->pendingDialogRequest = DialogRequest::NONE;
}

void IlluminatedProjectileDebugPanel::update(double dt)
{
    if (!this->loaded || !this->visible)
    {
        this->panelDragging = false;
        this->sliderDragging = false;
        this->trailScrollDragging = false;
        this->activeSliderIndex = -1;
        return;
    }

    const float dtf = (std::isfinite(dt) && dt > 0.0) ? static_cast<float>(dt) : 0.0f;
    this->previewAnimTimerSec += dtf;

    if (this->previewAssetsDirty ||
        this->loadedPreviewProjectileFolderPath != this->previewProjectileFolderPath)
    {
        this->previewProjectileVfx.unload();
        this->loadedPreviewProjectileFolderPath.clear();
        if (!this->previewProjectileFolderPath.empty() &&
            this->previewProjectileVfx.loadFromFolder(this->previewProjectileFolderPath.c_str()))
        {
            this->loadedPreviewProjectileFolderPath = this->previewProjectileFolderPath;
        }
    }
    if (this->previewAssetsDirty ||
        this->loadedPreviewTrailFolderPath != this->previewTrailFolderPath)
    {
        this->previewTrailVfx.unload();
        this->loadedPreviewTrailFolderPath.clear();
        if (!this->previewTrailFolderPath.empty() &&
            this->previewTrailVfx.loadFromFolder(this->previewTrailFolderPath.c_str()))
        {
            this->loadedPreviewTrailFolderPath = this->previewTrailFolderPath;
        }
    }
    this->previewAssetsDirty = false;

    if (this->previewProjectileVfx.isLoaded())
    {
        if (this->previewProjectileVfx.isFinished())
        {
            this->previewProjectileVfx.resetPlayback();
        }
        this->previewProjectileVfx.update(dtf);
    }
    if (this->previewTrailVfx.isLoaded())
    {
        if (this->previewTrailVfx.isFinished())
        {
            this->previewTrailVfx.resetPlayback();
        }
        this->previewTrailVfx.update(dtf);
    }

    if (this->statusMessageTimerSec > 0.0f)
    {
        this->statusMessageTimerSec = (std::max)(0.0f, this->statusMessageTimerSec - dtf);
        if (this->statusMessageTimerSec <= 0.0f)
        {
            this->statusMessage.clear();
        }
    }

    if (this->activePageIndex == 1)
    {
        applyTrajectoryPreviewPlacement(
            this->selectedDistanceBandIndex,
            this->selectedAngleSectorIndex);
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    if (this->panelDragging)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->panelDragging = false;
        }
        else
        {
            const SDL_FRect screenRect = getPanelScreenRect();
            const SDL_FRect clampedRect = clampRectInside(
                SDL_FRect{
                    mouseX - this->panelDragGrabOffsetX,
                    mouseY - this->panelDragGrabOffsetY,
                    (std::min)(kPanelWidth, screenRect.w - (kPanelMargin * 2.0f)),
                    (std::min)(kPanelHeight, screenRect.h - (kPanelMargin * 2.0f))
                },
                screenRect);
            this->panelOffset.x = clampedRect.x - (screenRect.x + kPanelMargin);
            this->panelOffset.y = clampedRect.y - (screenRect.y + kPanelMargin);
        }
    }

    const IlluminatedProjectileDebugPanelLayout layout = buildLayout(this->panelOffset, this->trailScrollOffsetY);
    this->trailScrollOffsetY = std::clamp(this->trailScrollOffsetY, 0.0f, layout.trailScrollMaxOffset);
    MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig trailConfig =
        MaritimeCannonSalvoSystem::getProjectileRibbonTrailConfig();
    if (this->selectedManualTrailStampIndex >= static_cast<int>(trailConfig.customStamps.size()))
    {
        this->selectedManualTrailStampIndex =
            trailConfig.customStamps.empty() ? -1 : (static_cast<int>(trailConfig.customStamps.size()) - 1);
    }

    if (this->trailScrollDragging)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->trailScrollDragging = false;
        }
        else if (layout.trailScrollMaxOffset > 1.0e-4f)
        {
            const float thumbTravel = (std::max)(0.0f, layout.trailScrollTrackRect.h - layout.trailScrollThumbRect.h);
            if (thumbTravel <= 1.0e-4f)
            {
                this->trailScrollOffsetY = 0.0f;
            }
            else
            {
                const float thumbY = std::clamp(
                    mouseY - this->trailScrollDragGrabOffsetY,
                    layout.trailScrollTrackRect.y,
                    layout.trailScrollTrackRect.y + thumbTravel);
                const float scrollT = (thumbY - layout.trailScrollTrackRect.y) / thumbTravel;
                this->trailScrollOffsetY = scrollT * layout.trailScrollMaxOffset;
            }
        }
    }

    if (this->manualTrailStampDragActive)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->manualTrailStampDragActive = false;
        }
        else if (this->activePageIndex == 2 &&
                 this->selectedManualTrailStampIndex >= 0 &&
                 this->selectedManualTrailStampIndex < static_cast<int>(trailConfig.customStamps.size()))
        {
            const SDL_FRect viewport = SDL_FRect{
                layout.trailManualPreviewRect.x + 10.0f,
                layout.trailManualPreviewRect.y + 50.0f,
                layout.trailManualPreviewRect.w - 20.0f,
                layout.trailManualPreviewRect.h - 60.0f
            };
            const std::vector<TrailPreviewPoint> manualPoints =
                buildTrailPreviewPoints(viewport, trailConfig, this->previewAnimTimerSec, true);
            auto& stamp = trailConfig.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)];
            float bestDist = stamp.distanceFromHeadTiles;
            float bestDist2 = (std::numeric_limits<float>::max)();
            TrailPreviewSample bestSample{};
            bool bestValid = false;
            for (int step = 0; step <= 160; ++step)
            {
                const float d = -trailConfig.headCoverTiles +
                    (((trailConfig.maxLengthTiles + trailConfig.headCoverTiles) * static_cast<float>(step)) / 160.0f);
                TrailPreviewSample sample{};
                if (!sampleTrailPreviewAtDistance(manualPoints, trailConfig, d, &sample))
                {
                    continue;
                }
                const float dx = mouseX - sample.position.x;
                const float dy = mouseY - sample.position.y;
                const float dist2 = (dx * dx) + (dy * dy);
                if (dist2 < bestDist2)
                {
                    bestDist2 = dist2;
                    bestDist = d;
                    bestSample = sample;
                    bestValid = true;
                }
            }
            if (bestValid)
            {
                const SDL_FPoint normal{-bestSample.tangent.y, bestSample.tangent.x};
                stamp.distanceFromHeadTiles = bestDist;
                stamp.lateralOffsetPixels =
                    ((mouseX - bestSample.position.x) * normal.x) + ((mouseY - bestSample.position.y) * normal.y);
                MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(trailConfig);
            }
        }
    }

    if (!this->sliderDragging)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->sliderDragging = false;
        this->activeSliderIndex = -1;
        return;
    }

    const int activeSliderCount =
        (this->activePageIndex == 1)
            ? kTrajectorySliderCount
            : ((this->activePageIndex == 2) ? (kManualTrailSliderIndexOffset + kManualTrailSliderCount) : kSliderCount);
    const bool activeShipSpeedSlider =
        this->activePageIndex == 1 &&
        this->activeSliderIndex == kShipSpeedActiveSliderIndex;
    if (!activeShipSpeedSlider &&
        (this->activeSliderIndex < 0 || this->activeSliderIndex >= activeSliderCount))
    {
        this->sliderDragging = false;
        this->activeSliderIndex = -1;
        return;
    }

    if (activeShipSpeedSlider)
    {
        const float nextValue = shipSpeedSliderValueFromTrackPosition(
            mouseX - this->sliderDragGrabOffsetX,
            layout.shipSpeedTrackRect);
        setDebugShipSpeedTilesPerSecond(nextValue);
    }
    else if (this->activePageIndex == 1)
    {
        MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning tuning =
            MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex);
        const SDL_FRect& trackRect =
            layout.trajectoryTrackRects[static_cast<size_t>(this->activeSliderIndex)];
        const float nextValue = trajectorySliderValueFromTrackPosition(
            this->activeSliderIndex,
            mouseX - this->sliderDragGrabOffsetX,
            trackRect);
        setTrajectorySliderValue(&tuning, this->activeSliderIndex, nextValue);
        MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
            this->selectedDistanceBandIndex,
            this->selectedAngleSectorIndex,
            tuning);
    }
    else if (this->activePageIndex == 2)
    {
        MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig config =
            MaritimeCannonSalvoSystem::getProjectileRibbonTrailConfig();
        if (this->activeSliderIndex >= kManualTrailSliderIndexOffset)
        {
            const int manualSliderIndex = this->activeSliderIndex - kManualTrailSliderIndexOffset;
            if (this->selectedManualTrailStampIndex >= 0 &&
                this->selectedManualTrailStampIndex < static_cast<int>(config.customStamps.size()) &&
                manualSliderIndex >= 0 &&
                manualSliderIndex < kManualTrailSliderCount)
            {
                const SDL_FRect& trackRect =
                    layout.trailManualSliderTrackRects[static_cast<size_t>(manualSliderIndex)];
                const float nextValue = manualTrailSliderValueFromTrackPosition(
                    manualSliderIndex,
                    mouseX - this->sliderDragGrabOffsetX,
                    trackRect);
                setManualTrailSliderValue(
                    &config.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)],
                    manualSliderIndex,
                    nextValue);
                MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            }
        }
        else
        {
            const SDL_FRect& trackRect =
                layout.trailTrackRects[static_cast<size_t>(this->activeSliderIndex)];
            const float nextValue = trailSliderValueFromTrackPosition(
                this->activeSliderIndex,
                mouseX - this->sliderDragGrabOffsetX,
                trackRect);
            setTrailSliderValue(&config, this->activeSliderIndex, nextValue);
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
        }
    }
    else
    {
        VFXClassic::IlluminatedProjectileGlowConfig config =
            VFXClassic::getIlluminatedProjectileGlowConfig();
        const SDL_FRect& trackRect = layout.trackRects[static_cast<size_t>(this->activeSliderIndex)];
        const float nextValue = sliderValueFromTrackPosition(
            this->activeSliderIndex,
            mouseX - this->sliderDragGrabOffsetX,
            trackRect);
        setSliderValue(&config, this->activeSliderIndex, nextValue);
        VFXClassic::setIlluminatedProjectileGlowConfig(config);
    }
}

void IlluminatedProjectileDebugPanel::draw(void) const
{
    if (!this->loaded || !this->visible)
    {
        return;
    }

    const IlluminatedProjectileDebugPanelLayout layout = buildLayout(this->panelOffset, this->trailScrollOffsetY);
    const VFXClassic::IlluminatedProjectileGlowConfig& config =
        VFXClassic::getIlluminatedProjectileGlowConfig();

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    if (this->activePageIndex == 1)
    {
        if (this->trajectoryCircleVisible)
        {
            drawTrajectoryAngleCompassOverlay(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex,
                const_cast<RC2D_Font*>(&this->bodyFont));
        }
        if (this->trajectoryDebugMask != 0U)
        {
            drawTrajectoryDebugOverlay(
                this->trajectoryDebugMask,
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex,
                const_cast<RC2D_Font*>(&this->bodyFont));
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    fillAndOutlineRect(layout.panelRect, kPanelFill, kPanelBorder);
    fillAndOutlineRect(layout.headerRect, kHeaderFill, kPanelBorder);
    fillAndOutlineRect(layout.badgeRect, kHeaderBadgeFill, kPanelBorder);
    fillAndOutlineRect(
        layout.hideButtonRect,
        pointInRect(mouseX, mouseY, layout.hideButtonRect) ? kButtonFillHover : kButtonFillSecondary,
        pointInRect(mouseX, mouseY, layout.hideButtonRect) ? kPanelBorder : kButtonBorderMuted);

    drawTextAt(
        const_cast<RC2D_Font*>(&this->titleFont),
        "Bullet FX Debug",
        layout.headerRect.x + 12.0f,
        layout.headerRect.y + 8.0f,
        kTextStrong);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "TEMP",
        layout.badgeRect,
        kTextStrong);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Hide",
        layout.hideButtonRect,
        kTextPrimary);

    for (int pageIndex = 0; pageIndex < kPageButtonCount; ++pageIndex)
    {
        const SDL_FRect& buttonRect = layout.pageButtonRects[static_cast<size_t>(pageIndex)];
        const bool selected = this->activePageIndex == pageIndex;
        const bool hovered = pointInRect(mouseX, mouseY, buttonRect);
        fillAndOutlineRect(
            buttonRect,
            selected
                ? (hovered ? kButtonFillHover : kButtonActiveFill)
                : (hovered ? kButtonFillHover : kButtonFillSecondary),
            selected ? kButtonActiveBorder : (hovered ? kPanelBorder : kButtonBorderMuted));
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            kPageLabels[static_cast<size_t>(pageIndex)],
            buttonRect,
            selected ? kTextStrong : kTextPrimary);
    }

    if (this->activePageIndex == 1)
    {
        const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning tuning =
            MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex);

        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Trajectoire gameplay exportable",
            layout.panelRect.x + 16.0f,
            layout.panelRect.y + 92.0f,
            kTextPrimary);

        {
            const float shipSpeed = getDebugShipSpeedTilesPerSecond();
            const bool rowHovered = pointInRect(mouseX, mouseY, layout.shipSpeedRowRect);
            const bool active =
                this->sliderDragging &&
                this->activePageIndex == 1 &&
                this->activeSliderIndex == kShipSpeedActiveSliderIndex;
            const SDL_FRect knobRect =
                buildShipSpeedSliderKnobRect(shipSpeed, layout.shipSpeedTrackRect);
            const float knobCenterX = knobRect.x + (knobRect.w * 0.5f);
            const SDL_FRect valueFillRect = SDL_FRect{
                layout.shipSpeedTrackRect.x,
                layout.shipSpeedTrackRect.y,
                (std::max)(0.0f, knobCenterX - layout.shipSpeedTrackRect.x),
                layout.shipSpeedTrackRect.h
            };

            fillAndOutlineRect(
                layout.shipSpeedRowRect,
                kRowFill,
                rowHovered ? kPanelBorder : kButtonBorderMuted);
            fillAndOutlineRect(
                layout.shipSpeedTrackRect,
                active ? kTrackFillActive : (rowHovered ? kTrackFillHover : kTrackFill),
                rowHovered ? kPanelBorder : kTrackBorder);
            if (valueFillRect.w > 0.0f)
            {
                rc2d_graphics_setColor(kTrackValueFill);
                rc2d_graphics_rectangle("fill", &valueFillRect);
            }
            fillAndOutlineRect(knobRect, kKnobFill, kKnobBorder);

            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                kShipSpeedSliderSpec.label,
                layout.shipSpeedRowRect.x + 10.0f,
                layout.shipSpeedRowRect.y + 6.0f,
                kTextPrimary);

            const std::string valueText = formatSliderValue(shipSpeed);
            const SDL_FRect valueRect = SDL_FRect{
                layout.shipSpeedTrackRect.x + layout.shipSpeedTrackRect.w + 12.0f,
                layout.shipSpeedRowRect.y,
                layout.shipSpeedRowRect.x + layout.shipSpeedRowRect.w -
                    (layout.shipSpeedTrackRect.x + layout.shipSpeedTrackRect.w + 12.0f),
                layout.shipSpeedRowRect.h
            };
            drawRightAlignedText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                valueText.c_str(),
                valueRect,
                active ? kTextStrong : kTextMuted);
        }

        fillAndOutlineRect(layout.distanceBandRowRect, kRowFill, kButtonBorderMuted);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Distance tuiles",
            layout.distanceBandRowRect.x + 10.0f,
            layout.distanceBandRowRect.y + 6.0f,
            kTextPrimary);
        for (int index = 0; index < kDistanceBandButtonCount; ++index)
        {
            const SDL_FRect& buttonRect = layout.distanceBandButtonRects[static_cast<size_t>(index)];
            const bool selected = this->selectedDistanceBandIndex == index;
            const bool hovered = pointInRect(mouseX, mouseY, buttonRect);
            fillAndOutlineRect(
                buttonRect,
                selected
                    ? (hovered ? kButtonFillHover : kButtonActiveFill)
                    : (hovered ? kButtonFillHover : kButtonFillSecondary),
                selected ? kButtonActiveBorder : (hovered ? kPanelBorder : kButtonBorderMuted));
            drawCenteredText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                kDistanceBandLabels[static_cast<size_t>(index)],
                buttonRect,
                selected ? kTextStrong : kTextPrimary);
        }

        fillAndOutlineRect(layout.angleSectorRowRect, kRowFill, kButtonBorderMuted);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Angle deg",
            layout.angleSectorRowRect.x + 10.0f,
            layout.angleSectorRowRect.y + 6.0f,
            kTextPrimary);
        for (int index = 0; index < kAngleSectorButtonCount; ++index)
        {
            const SDL_FRect& buttonRect = layout.angleSectorButtonRects[static_cast<size_t>(index)];
            const bool selected = this->selectedAngleSectorIndex == index;
            const bool hovered = pointInRect(mouseX, mouseY, buttonRect);
            fillAndOutlineRect(
                buttonRect,
                selected
                    ? (hovered ? kButtonFillHover : kButtonActiveFill)
                    : (hovered ? kButtonFillHover : kButtonFillSecondary),
                selected ? kButtonActiveBorder : (hovered ? kPanelBorder : kButtonBorderMuted));
            drawCenteredText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                kAngleSectorLabels[static_cast<size_t>(index)],
                buttonRect,
                selected ? kTextStrong : kTextPrimary);
        }

        for (int index = 0; index < kTrajectorySliderCount; ++index)
        {
            const SDL_FRect& rowRect = layout.trajectoryRowRects[static_cast<size_t>(index)];
            const SDL_FRect& trackRect = layout.trajectoryTrackRects[static_cast<size_t>(index)];
            const SDL_FRect& debugButtonRect =
                layout.trajectoryDebugButtonRects[static_cast<size_t>(index)];
            const bool rowHovered = pointInRect(mouseX, mouseY, rowRect);
            const bool debugHovered = pointInRect(mouseX, mouseY, debugButtonRect);
            const bool debugEnabled =
                isTrajectoryDebugEnabled(this->trajectoryDebugMask, index);
            const bool active =
                this->sliderDragging &&
                this->activePageIndex == 1 &&
                this->activeSliderIndex == index;
            const float sliderValue = getTrajectorySliderValue(tuning, index);

            fillAndOutlineRect(rowRect, kRowFill, rowHovered ? kPanelBorder : kButtonBorderMuted);

            if (isArcSideControlIndex(index))
            {
                const SDL_FRect leftRect = buildArcSideChoiceRect(trackRect, false);
                const SDL_FRect rightRect = buildArcSideChoiceRect(trackRect, true);
                const bool leftHovered = pointInRect(mouseX, mouseY, leftRect);
                const bool rightHovered = pointInRect(mouseX, mouseY, rightRect);
                const bool leftActive = normalizeArcSideSelection(sliderValue) < 0.0f;
                fillAndOutlineRect(
                    leftRect,
                    leftActive
                        ? (leftHovered ? kButtonFillHover : kButtonActiveFill)
                        : (leftHovered ? kButtonFillHover : kButtonFillSecondary),
                    leftActive ? kButtonActiveBorder : (leftHovered ? kPanelBorder : kButtonBorderMuted));
                fillAndOutlineRect(
                    rightRect,
                    !leftActive
                        ? (rightHovered ? kButtonFillHover : kButtonActiveFill)
                        : (rightHovered ? kButtonFillHover : kButtonFillSecondary),
                    !leftActive ? kButtonActiveBorder : (rightHovered ? kPanelBorder : kButtonBorderMuted));
                drawCenteredText(
                    const_cast<RC2D_Font*>(&this->bodyFont),
                    "Gauche",
                    leftRect,
                    leftActive ? kTextStrong : kTextPrimary);
                drawCenteredText(
                    const_cast<RC2D_Font*>(&this->bodyFont),
                    "Droite",
                    rightRect,
                    !leftActive ? kTextStrong : kTextPrimary);
            }
            else
            {
                const SDL_FRect knobRect = buildTrajectorySliderKnobRect(index, sliderValue, trackRect);
                const float knobCenterX = knobRect.x + (knobRect.w * 0.5f);
                const SDL_FRect valueFillRect = SDL_FRect{
                    trackRect.x,
                    trackRect.y,
                    (std::max)(0.0f, knobCenterX - trackRect.x),
                    trackRect.h
                };

                fillAndOutlineRect(
                    trackRect,
                    active ? kTrackFillActive : (rowHovered ? kTrackFillHover : kTrackFill),
                    rowHovered ? kPanelBorder : kTrackBorder);
                if (valueFillRect.w > 0.0f)
                {
                    rc2d_graphics_setColor(kTrackValueFill);
                    rc2d_graphics_rectangle("fill", &valueFillRect);
                }
                fillAndOutlineRect(knobRect, kKnobFill, kKnobBorder);
            }

            fillAndOutlineRect(
                debugButtonRect,
                debugEnabled
                    ? (debugHovered ? kButtonFillHover : kButtonActiveFill)
                    : (debugHovered ? kButtonFillHover : kButtonFillSecondary),
                debugEnabled ? kButtonActiveBorder : (debugHovered ? kPanelBorder : kButtonBorderMuted));

            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                kTrajectorySliderSpecs[static_cast<size_t>(index)].label,
                rowRect.x + 10.0f,
                rowRect.y + 6.0f,
                kTextPrimary);
            drawCenteredText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                "Debug",
                debugButtonRect,
                debugEnabled ? kTextStrong : kTextPrimary);

            const std::string valueText =
                isArcSideControlIndex(index)
                    ? std::string(getArcSideSelectionLabel(sliderValue))
                    : formatSliderValue(sliderValue);
            const float valueX = debugButtonRect.x + debugButtonRect.w + 8.0f;
            const SDL_FRect valueRect = SDL_FRect{
                valueX,
                rowRect.y,
                rowRect.x + rowRect.w - valueX,
                rowRect.h
            };
            drawRightAlignedText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                valueText.c_str(),
                valueRect,
                active ? kTextStrong : kTextMuted);
        }

        fillAndOutlineRect(
            layout.trajectoryExportButtonRect,
            pointInRect(mouseX, mouseY, layout.trajectoryExportButtonRect) ? kButtonFillHover : kButtonFill,
            pointInRect(mouseX, mouseY, layout.trajectoryExportButtonRect) ? kPanelBorder : kPanelBorder);
        fillAndOutlineRect(
            layout.trajectoryResetSelectedButtonRect,
            pointInRect(mouseX, mouseY, layout.trajectoryResetSelectedButtonRect) ? kButtonFillHover : kButtonFillSecondary,
            pointInRect(mouseX, mouseY, layout.trajectoryResetSelectedButtonRect) ? kPanelBorder : kButtonBorderMuted);
        fillAndOutlineRect(
            layout.trajectoryResetAllButtonRect,
            pointInRect(mouseX, mouseY, layout.trajectoryResetAllButtonRect) ? kButtonFillHover : kButtonFillSecondary,
            pointInRect(mouseX, mouseY, layout.trajectoryResetAllButtonRect) ? kPanelBorder : kButtonBorderMuted);
        fillAndOutlineRect(
            layout.trajectoryCircleToggleButtonRect,
            this->trajectoryCircleVisible
                ? (pointInRect(mouseX, mouseY, layout.trajectoryCircleToggleButtonRect) ? kButtonFillHover : kButtonActiveFill)
                : (pointInRect(mouseX, mouseY, layout.trajectoryCircleToggleButtonRect) ? kButtonFillHover : kButtonFillSecondary),
            this->trajectoryCircleVisible
                ? kButtonActiveBorder
                : (pointInRect(mouseX, mouseY, layout.trajectoryCircleToggleButtonRect) ? kPanelBorder : kButtonBorderMuted));

        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Export JSON",
            layout.trajectoryExportButtonRect,
            kTextPrimary);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Reset case",
            layout.trajectoryResetSelectedButtonRect,
            kTextPrimary);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Reset all",
            layout.trajectoryResetAllButtonRect,
            kTextPrimary);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            this->trajectoryCircleVisible ? "Cercle ON" : "Cercle OFF",
            layout.trajectoryCircleToggleButtonRect,
            this->trajectoryCircleVisible ? kTextStrong : kTextPrimary);

        if (!this->statusMessage.empty())
        {
            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                this->statusMessage.c_str(),
                layout.panelRect.x + 16.0f,
                layout.trajectoryExportButtonRect.y - 22.0f,
                kTextStrong);
        }
        else
        {
            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                MaritimeCannonSalvoSystem::getProjectileTrajectoryConfigPath(),
                layout.panelRect.x + 16.0f,
                layout.trajectoryExportButtonRect.y - 22.0f,
                kTextMuted);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    if (this->activePageIndex == 2)
    {
        const MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig trailConfig =
            MaritimeCannonSalvoSystem::getProjectileRibbonTrailConfig();
        const HelpEntry* hoveredTrailHelp = &kTrailHelpDefault;
        const std::string projectilePreviewLabel =
            getFolderLeafLabel(this->previewProjectileFolderPath);
        const std::string trailPreviewLabel =
            getFolderLeafLabel(this->previewTrailFolderPath);

        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Ribbon trail exportable pour les boulets de salve",
            layout.panelRect.x + 16.0f,
            layout.panelRect.y + 92.0f,
            kTextPrimary);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "La spritesheet trail se choisit dans la liste Trail VFX a droite.",
            layout.panelRect.x + 16.0f,
            layout.panelRect.y + 144.0f,
            kTextMuted);

        const bool trailEnabled = trailConfig.meshEnabled || trailConfig.stampsEnabled;
        const bool trailMeshEnabled = trailConfig.meshEnabled;
        const bool trailStampsEnabled = trailConfig.stampsEnabled;
        const bool trailBlendAdditive = trailConfig.additiveStampBlend;
        const auto drawTrailToggle = [&](const SDL_FRect& rect, const char* label, bool enabled) {
            const bool hovered = pointInRect(mouseX, mouseY, rect);
            fillAndOutlineRect(
                rect,
                enabled
                    ? (hovered ? kButtonFillHover : kButtonActiveFill)
                    : (hovered ? kButtonFillHover : kButtonFillSecondary),
                enabled ? kButtonActiveBorder : (hovered ? kPanelBorder : kButtonBorderMuted));
            drawCenteredText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                label,
                rect,
                enabled ? kTextStrong : kTextPrimary);
        };

        drawTrailToggle(layout.trailEnableButtonRect, trailEnabled ? "Trail ON" : "Trail OFF", trailEnabled);
        drawTrailToggle(layout.trailMeshToggleButtonRect, trailMeshEnabled ? "Mesh ON" : "Mesh OFF", trailMeshEnabled);
        drawTrailToggle(layout.trailStampsToggleButtonRect, trailStampsEnabled ? "Sprites ON" : "Sprites OFF", trailStampsEnabled);
        drawTrailToggle(layout.trailBlendToggleButtonRect, trailBlendAdditive ? "Blend ADD" : "Blend ALPHA", trailBlendAdditive);
        if (pointInRect(mouseX, mouseY, layout.trailEnableButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpTrailToggle;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailMeshToggleButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpMeshToggle;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailStampsToggleButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpSpritesToggle;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailBlendToggleButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpBlendToggle;
        }

        drawTrailToggle(
            layout.trailManualModeButtonRect,
            trailConfig.manualStampsEnabled ? "Manual ON" : "Manual OFF",
            trailConfig.manualStampsEnabled);
        drawTrailToggle(
            layout.trailManualAddButtonRect,
            "+ Stamp",
            false);
        drawTrailToggle(
            layout.trailManualDuplicateButtonRect,
            "Dupli",
            false);
        drawTrailToggle(
            layout.trailManualDeleteButtonRect,
            "- Stamp",
            false);
        drawTrailToggle(
            layout.trailManualClearButtonRect,
            "Clear",
            false);
        if (pointInRect(mouseX, mouseY, layout.trailManualModeButtonRect) ||
            pointInRect(mouseX, mouseY, layout.trailManualPreviewRect))
        {
            hoveredTrailHelp =
                pointInRect(mouseX, mouseY, layout.trailManualModeButtonRect)
                    ? &kTrailHelpManualMode
                    : &kTrailHelpManualPreview;
        }

        SDL_Renderer* trailRenderer = SDL_GetRenderer(rc2d_window_getWindow());
        if (trailRenderer != nullptr)
        {
            const SDL_Rect clipRect = toClipRect(layout.trailControlsViewportRect);
            SDL_SetRenderClipRect(trailRenderer, &clipRect);
        }
        for (int index = 0; index < kTrailSliderCount; ++index)
        {
            const SDL_FRect& rowRect = layout.trailRowRects[static_cast<size_t>(index)];
            const SDL_FRect& trackRect = layout.trailTrackRects[static_cast<size_t>(index)];
            if (!rectsIntersect(rowRect, layout.trailControlsViewportRect))
            {
                continue;
            }

            const bool rowHovered =
                pointInRect(mouseX, mouseY, rowRect) &&
                pointInRect(mouseX, mouseY, layout.trailControlsViewportRect);
            const bool active =
                this->sliderDragging &&
                this->activePageIndex == 2 &&
                this->activeSliderIndex == index;
            const float sliderValue = getTrailSliderValue(trailConfig, index);
            if (rowHovered)
            {
                hoveredTrailHelp = &getTrailSliderHelpEntry(index);
            }
            const SDL_FRect knobRect = buildTrailSliderKnobRect(index, sliderValue, trackRect);
            const float knobCenterX = knobRect.x + (knobRect.w * 0.5f);
            const SDL_FRect valueFillRect = SDL_FRect{
                trackRect.x,
                trackRect.y,
                (std::max)(0.0f, knobCenterX - trackRect.x),
                trackRect.h
            };

            fillAndOutlineRect(rowRect, kRowFill, rowHovered ? kPanelBorder : kButtonBorderMuted);
            fillAndOutlineRect(
                trackRect,
                active ? kTrackFillActive : (rowHovered ? kTrackFillHover : kTrackFill),
                rowHovered ? kPanelBorder : kTrackBorder);
            if (valueFillRect.w > 0.0f)
            {
                rc2d_graphics_setColor(kTrackValueFill);
                rc2d_graphics_rectangle("fill", &valueFillRect);
            }
            fillAndOutlineRect(knobRect, kKnobFill, kKnobBorder);

            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                kTrailSliderSpecs[static_cast<size_t>(index)].label,
                rowRect.x + 10.0f,
                rowRect.y + 6.0f,
                kTextPrimary);

            const std::string valueText = formatSliderValue(sliderValue);
            const SDL_FRect valueRect = SDL_FRect{
                trackRect.x + trackRect.w + 12.0f,
                rowRect.y,
                rowRect.x + rowRect.w - (trackRect.x + trackRect.w + 12.0f),
                rowRect.h
            };
            drawRightAlignedText(
                const_cast<RC2D_Font*>(&this->bodyFont),
                valueText.c_str(),
                valueRect,
                active ? kTextStrong : kTextMuted);
        }
        if (trailRenderer != nullptr)
        {
            SDL_SetRenderClipRect(trailRenderer, nullptr);
        }
        fillAndOutlineRect(layout.trailScrollTrackRect, kTrackFill, kTrackBorder);
        fillAndOutlineRect(
            layout.trailScrollThumbRect,
            (layout.trailScrollMaxOffset > 1.0e-4f)
                ? (pointInRect(mouseX, mouseY, layout.trailScrollThumbRect) || this->trailScrollDragging ? kButtonFillHover : kKnobFill)
                : kButtonFillSecondary,
            (layout.trailScrollMaxOffset > 1.0e-4f) ? kKnobBorder : kButtonBorderMuted);

        fillAndOutlineRect(
            layout.trailImportButtonRect,
            pointInRect(mouseX, mouseY, layout.trailImportButtonRect) ? kButtonFillHover : kButtonFill,
            pointInRect(mouseX, mouseY, layout.trailImportButtonRect) ? kPanelBorder : kPanelBorder);
        fillAndOutlineRect(
            layout.trailExportButtonRect,
            pointInRect(mouseX, mouseY, layout.trailExportButtonRect) ? kButtonFillHover : kButtonFill,
            pointInRect(mouseX, mouseY, layout.trailExportButtonRect) ? kPanelBorder : kPanelBorder);
        fillAndOutlineRect(
            layout.trailResetButtonRect,
            pointInRect(mouseX, mouseY, layout.trailResetButtonRect) ? kButtonFillHover : kButtonFill,
            pointInRect(mouseX, mouseY, layout.trailResetButtonRect) ? kPanelBorder : kPanelBorder);
        fillAndOutlineRect(
            layout.trailFooterHideButtonRect,
            pointInRect(mouseX, mouseY, layout.trailFooterHideButtonRect) ? kButtonFillHover : kButtonFillSecondary,
            pointInRect(mouseX, mouseY, layout.trailFooterHideButtonRect) ? kPanelBorder : kButtonBorderMuted);
        if (pointInRect(mouseX, mouseY, layout.trailImportButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpImport;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailExportButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpExport;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailResetButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpReset;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailFooterHideButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpHide;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailPreviewRect))
        {
            hoveredTrailHelp = &kTrailHelpPreview;
        }
        else if (pointInRect(mouseX, mouseY, layout.trailManualAddButtonRect) ||
                 pointInRect(mouseX, mouseY, layout.trailManualDuplicateButtonRect) ||
                 pointInRect(mouseX, mouseY, layout.trailManualDeleteButtonRect) ||
                 pointInRect(mouseX, mouseY, layout.trailManualClearButtonRect))
        {
            hoveredTrailHelp = &kTrailHelpManualPreview;
        }

        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Import JSON",
            layout.trailImportButtonRect,
            kTextPrimary);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Export JSON",
            layout.trailExportButtonRect,
            kTextPrimary);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Reset trail",
            layout.trailResetButtonRect,
            kTextPrimary);
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            "Hide (F2)",
            layout.trailFooterHideButtonRect,
            kTextPrimary);

        drawTrailPreviewPanel(
            layout.trailPreviewRect,
            trailConfig,
            this->previewProjectileVfx.isLoaded() ? &this->previewProjectileVfx : nullptr,
            this->previewTrailVfx.isLoaded() ? &this->previewTrailVfx : nullptr,
            this->previewProjectileIlluminated,
            this->previewTrailEnabled,
            this->previewAnimTimerSec,
            const_cast<RC2D_Font*>(&this->bodyFont),
            projectilePreviewLabel,
            trailPreviewLabel,
            &layout.trailManualPreviewRect,
            this->selectedManualTrailStampIndex);

        for (int index = 0; index < kManualTrailSliderCount; ++index)
        {
            const SDL_FRect& rowRect = layout.trailManualSliderRowRects[static_cast<size_t>(index)];
            const SDL_FRect& trackRect = layout.trailManualSliderTrackRects[static_cast<size_t>(index)];
            const bool rowHovered = pointInRect(mouseX, mouseY, rowRect);
            const bool active =
                this->sliderDragging &&
                this->activePageIndex == 2 &&
                this->activeSliderIndex == (kManualTrailSliderIndexOffset + index);
            fillAndOutlineRect(rowRect, kRowFill, rowHovered ? kPanelBorder : kButtonBorderMuted);
            fillAndOutlineRect(
                trackRect,
                active ? kTrackFillActive : (rowHovered ? kTrackFillHover : kTrackFill),
                rowHovered ? kPanelBorder : kTrackBorder);
            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                kManualTrailSliderSpecs[static_cast<size_t>(index)].label,
                rowRect.x + 10.0f,
                rowRect.y + 6.0f,
                this->selectedManualTrailStampIndex >= 0 ? kTextPrimary : kTextMuted);

            if (this->selectedManualTrailStampIndex >= 0 &&
                this->selectedManualTrailStampIndex < static_cast<int>(trailConfig.customStamps.size()))
            {
                const auto& stamp = trailConfig.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)];
                const float sliderValue = getManualTrailSliderValue(stamp, index);
                const SDL_FRect knobRect = buildManualTrailSliderKnobRect(index, sliderValue, trackRect);
                const SDL_FRect valueFillRect = SDL_FRect{
                    trackRect.x,
                    trackRect.y,
                    (std::max)(0.0f, (knobRect.x + (knobRect.w * 0.5f)) - trackRect.x),
                    trackRect.h
                };
                if (valueFillRect.w > 0.0f)
                {
                    rc2d_graphics_setColor(kTrackValueFill);
                    rc2d_graphics_rectangle("fill", &valueFillRect);
                }
                fillAndOutlineRect(knobRect, kKnobFill, kKnobBorder);
                const std::string valueText = formatSliderValue(sliderValue);
                drawRightAlignedText(
                    const_cast<RC2D_Font*>(&this->bodyFont),
                    valueText.c_str(),
                    SDL_FRect{trackRect.x + trackRect.w + 12.0f, rowRect.y, rowRect.w - (trackRect.x + trackRect.w - rowRect.x) - 12.0f, rowRect.h},
                    active ? kTextStrong : kTextMuted);
            }
            else
            {
                drawRightAlignedText(
                    const_cast<RC2D_Font*>(&this->bodyFont),
                    "-",
                    SDL_FRect{trackRect.x + trackRect.w + 12.0f, rowRect.y, rowRect.w - (trackRect.x + trackRect.w - rowRect.x) - 12.0f, rowRect.h},
                    kTextMuted);
            }
        }

        fillAndOutlineRect(layout.trailHelpRect, kHelpFill, kHelpBorder);
        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            hoveredTrailHelp->title,
            layout.trailHelpRect.x + 12.0f,
            layout.trailHelpRect.y + 10.0f,
            kHelpAccent);
        drawWrappedText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            hoveredTrailHelp->body,
            SDL_FRect{
                layout.trailHelpRect.x + 12.0f,
                layout.trailHelpRect.y + 32.0f,
                layout.trailHelpRect.w - 24.0f,
                layout.trailHelpRect.h - 44.0f
            },
            kTextPrimary,
            2.0f);

        if (!this->statusMessage.empty())
        {
            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                this->statusMessage.c_str(),
                layout.panelRect.x + 16.0f,
                layout.trailExportButtonRect.y - 22.0f,
                kTextStrong);
        }
        else
        {
            std::string trailSourceText = "mode trail local (config locale)";
            if (!gEditorTrailSourcePath.empty())
            {
                trailSourceText = gEditorTrailSourcePath;
            }
            drawTextAt(
                const_cast<RC2D_Font*>(&this->bodyFont),
                trailSourceText.c_str(),
                layout.panelRect.x + 16.0f,
                layout.trailExportButtonRect.y - 22.0f,
                kTextMuted);
        }

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        return;
    }

    drawTextAt(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "TEMP DEBUG TOOL - live tuning des boulets illumines",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 92.0f,
        kTextPrimary);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Affecte les boulets en vol et les nouveaux tirs.",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 110.0f,
        kTextMuted);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "F2 masque/affiche le panneau. Compact glow + core sharpness inclus.",
        layout.panelRect.x + 16.0f,
        layout.panelRect.y + 126.0f,
        kTextMuted);

    if (!this->statusMessage.empty())
    {
        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            this->statusMessage.c_str(),
            layout.panelRect.x + 16.0f,
            layout.glowExportButtonRect.y - 22.0f,
            kTextStrong);
    }
    else
    {
        std::string glowSourceText = "mode glow live (config locale)";
        if (gEditorGlowModeDefault)
        {
            glowSourceText = "assets/data/ammo-illu-default.json";
        }
        else if (!gEditorGlowLiveSourcePath.empty())
        {
            glowSourceText = gEditorGlowLiveSourcePath;
        }
        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            glowSourceText.c_str(),
            layout.panelRect.x + 16.0f,
            layout.glowExportButtonRect.y - 22.0f,
            kTextMuted);
    }

    for (int index = 0; index < kSliderCount; ++index)
    {
        const SDL_FRect& rowRect = layout.rowRects[static_cast<size_t>(index)];
        const SDL_FRect& trackRect = layout.trackRects[static_cast<size_t>(index)];
        const bool rowHovered = pointInRect(mouseX, mouseY, rowRect);
        const bool active = this->sliderDragging && this->activeSliderIndex == index;
        const float sliderValue = getSliderValue(config, index);
        const SDL_FRect knobRect = buildSliderKnobRect(index, sliderValue, trackRect);
        const float knobCenterX = knobRect.x + (knobRect.w * 0.5f);
        const SDL_FRect valueFillRect = SDL_FRect{
            trackRect.x,
            trackRect.y,
            (std::max)(0.0f, knobCenterX - trackRect.x),
            trackRect.h
        };

        fillAndOutlineRect(rowRect, kRowFill, rowHovered ? kPanelBorder : kButtonBorderMuted);
        fillAndOutlineRect(
            trackRect,
            active ? kTrackFillActive : (rowHovered ? kTrackFillHover : kTrackFill),
            rowHovered ? kPanelBorder : kTrackBorder);
        if (valueFillRect.w > 0.0f)
        {
            rc2d_graphics_setColor(kTrackValueFill);
            rc2d_graphics_rectangle("fill", &valueFillRect);
        }
        fillAndOutlineRect(knobRect, kKnobFill, kKnobBorder);

        drawTextAt(
            const_cast<RC2D_Font*>(&this->bodyFont),
            kSliderSpecs[static_cast<size_t>(index)].label,
            rowRect.x + 10.0f,
            rowRect.y + 6.0f,
            kTextPrimary);

        const std::string valueText = formatSliderValue(sliderValue);
        const SDL_FRect valueRect = SDL_FRect{
            trackRect.x + trackRect.w + 12.0f,
            rowRect.y,
            rowRect.x + rowRect.w - (trackRect.x + trackRect.w + 12.0f),
            rowRect.h
        };
        drawRightAlignedText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            valueText.c_str(),
            valueRect,
            active ? kTextStrong : kTextMuted);
    }

    fillAndOutlineRect(layout.layerRowRect, kRowFill, kButtonBorderMuted);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Couches glow",
        layout.layerRowRect.x + 10.0f,
        layout.layerRowRect.y + 6.0f,
        kTextPrimary);
    for (int layerIndex = 0; layerIndex < kLayerButtonCount; ++layerIndex)
    {
        const SDL_FRect& buttonRect = layout.layerButtonRects[static_cast<size_t>(layerIndex)];
        const bool enabled = getLayerEnabled(config, layerIndex);
        const bool hovered = pointInRect(mouseX, mouseY, buttonRect);
        fillAndOutlineRect(
            buttonRect,
            enabled
                ? (hovered ? kButtonFillHover : kButtonActiveFill)
                : (hovered ? kButtonFillHover : kButtonFillSecondary),
            enabled ? kButtonActiveBorder : (hovered ? kPanelBorder : kButtonBorderMuted));
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            kLayerLabels[static_cast<size_t>(layerIndex)],
            buttonRect,
            enabled ? kTextStrong : kTextPrimary);
    }

    fillAndOutlineRect(layout.compactRowRect, kRowFill, kButtonBorderMuted);
    drawTextAt(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Compact glow",
        layout.compactRowRect.x + 10.0f,
        layout.compactRowRect.y + 6.0f,
        kTextPrimary);
    {
        const bool compactGlowEnabled = getCompactGlowEnabled(config);
        const bool compactHovered = pointInRect(mouseX, mouseY, layout.compactToggleRect);
        fillAndOutlineRect(
            layout.compactToggleRect,
            compactGlowEnabled
                ? (compactHovered ? kButtonFillHover : kButtonActiveFill)
                : (compactHovered ? kButtonFillHover : kButtonFillSecondary),
            compactGlowEnabled
                ? kButtonActiveBorder
                : (compactHovered ? kPanelBorder : kButtonBorderMuted));
        drawCenteredText(
            const_cast<RC2D_Font*>(&this->bodyFont),
            compactGlowEnabled ? "Enabled" : "Disabled",
            layout.compactToggleRect,
            compactGlowEnabled ? kTextStrong : kTextPrimary);
    }

    fillAndOutlineRect(
        layout.glowImportButtonRect,
        pointInRect(mouseX, mouseY, layout.glowImportButtonRect) ? kButtonFillHover : kButtonFill,
        pointInRect(mouseX, mouseY, layout.glowImportButtonRect) ? kPanelBorder : kPanelBorder);
    fillAndOutlineRect(
        layout.glowExportButtonRect,
        pointInRect(mouseX, mouseY, layout.glowExportButtonRect) ? kButtonFillHover : kButtonFill,
        pointInRect(mouseX, mouseY, layout.glowExportButtonRect) ? kPanelBorder : kPanelBorder);
    fillAndOutlineRect(
        layout.resetButtonRect,
        pointInRect(mouseX, mouseY, layout.resetButtonRect) ? kButtonFillHover : kButtonFill,
        pointInRect(mouseX, mouseY, layout.resetButtonRect) ? kPanelBorder : kPanelBorder);
    fillAndOutlineRect(
        layout.footerHideButtonRect,
        pointInRect(mouseX, mouseY, layout.footerHideButtonRect) ? kButtonFillHover : kButtonFillSecondary,
        pointInRect(mouseX, mouseY, layout.footerHideButtonRect) ? kPanelBorder : kButtonBorderMuted);

    drawCenteredText(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Import JSON",
        layout.glowImportButtonRect,
        kTextPrimary);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Export JSON",
        layout.glowExportButtonRect,
        kTextPrimary);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Reset glow default",
        layout.resetButtonRect,
        kTextPrimary);
    drawCenteredText(
        const_cast<RC2D_Font*>(&this->bodyFont),
        "Hide (F2)",
        layout.footerHideButtonRect,
        kTextPrimary);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool IlluminatedProjectileDebugPanel::mousepressed(float x, float y, RC2D_MouseButton button)
{
    if (!this->loaded || !this->visible)
    {
        return false;
    }

    const IlluminatedProjectileDebugPanelLayout layout = buildLayout(this->panelOffset, this->trailScrollOffsetY);
    if (!pointInRect(x, y, layout.panelRect))
    {
        return false;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    this->panelDragging = false;
    this->sliderDragging = false;
    this->manualTrailStampDragActive = false;
    this->activeSliderIndex = -1;

    if (pointInRect(x, y, layout.hideButtonRect) ||
        (this->activePageIndex == 0 && pointInRect(x, y, layout.footerHideButtonRect)) ||
        (this->activePageIndex == 2 && pointInRect(x, y, layout.trailFooterHideButtonRect)))
    {
        this->setVisible(false);
        return true;
    }

    for (int pageIndex = 0; pageIndex < kPageButtonCount; ++pageIndex)
    {
        if (!pointInRect(x, y, layout.pageButtonRects[static_cast<size_t>(pageIndex)]))
        {
            continue;
        }

        this->activePageIndex = pageIndex;
        this->sliderDragging = false;
        this->activeSliderIndex = -1;
        if (this->activePageIndex == 1)
        {
            applyTrajectoryPreviewPlacement(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex);
        }
        return true;
    }

    if (this->activePageIndex == 1)
    {
        if (pointInRect(x, y, layout.shipSpeedRowRect))
        {
            this->sliderDragging = true;
            this->activeSliderIndex = kShipSpeedActiveSliderIndex;
            this->sliderDragGrabOffsetX = 0.0f;

            const float nextValue =
                shipSpeedSliderValueFromTrackPosition(x, layout.shipSpeedTrackRect);
            setDebugShipSpeedTilesPerSecond(nextValue);
            return true;
        }

        for (int index = 0; index < kDistanceBandButtonCount; ++index)
        {
            if (pointInRect(x, y, layout.distanceBandButtonRects[static_cast<size_t>(index)]))
            {
                this->selectedDistanceBandIndex = index;
                this->sliderDragging = false;
                this->activeSliderIndex = -1;
                applyTrajectoryPreviewPlacement(
                    this->selectedDistanceBandIndex,
                    this->selectedAngleSectorIndex);
                return true;
            }
        }

        for (int index = 0; index < kAngleSectorButtonCount; ++index)
        {
            if (pointInRect(x, y, layout.angleSectorButtonRects[static_cast<size_t>(index)]))
            {
                this->selectedAngleSectorIndex = index;
                this->sliderDragging = false;
                this->activeSliderIndex = -1;
                applyTrajectoryPreviewPlacement(
                    this->selectedDistanceBandIndex,
                    this->selectedAngleSectorIndex);
                return true;
            }
        }

        if (pointInRect(x, y, layout.trajectoryExportButtonRect))
        {
            const bool exported =
                MaritimeCannonSalvoSystem::exportProjectileTrajectoryTuningsToFile();
            this->statusMessage =
                exported
                    ? (std::string("Export OK: ") + MaritimeCannonSalvoSystem::getProjectileTrajectoryConfigPath())
                    : "Export impossible.";
            this->statusMessageTimerSec = 4.0f;
            return true;
        }

        if (pointInRect(x, y, layout.trajectoryResetSelectedButtonRect))
        {
            const MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning defaults =
                MaritimeCannonSalvoSystem::getDefaultProjectileTrajectoryTuning(
                    this->selectedDistanceBandIndex,
                    this->selectedAngleSectorIndex);
            MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex,
                defaults);
            this->statusMessage = "Case trajectoire reinitialisee.";
            this->statusMessageTimerSec = 2.0f;
            return true;
        }

        if (pointInRect(x, y, layout.trajectoryResetAllButtonRect))
        {
            MaritimeCannonSalvoSystem::resetProjectileTrajectoryTunings();
            this->statusMessage = "Toutes les trajectoires sont par defaut.";
            this->statusMessageTimerSec = 2.0f;
            return true;
        }

        if (pointInRect(x, y, layout.trajectoryCircleToggleButtonRect))
        {
            this->trajectoryCircleVisible = !this->trajectoryCircleVisible;
            this->statusMessage =
                this->trajectoryCircleVisible ? "Cercle angles affiche." : "Cercle angles masque.";
            this->statusMessageTimerSec = 1.5f;
            return true;
        }

        for (int sliderIndex = 0; sliderIndex < kTrajectorySliderCount; ++sliderIndex)
        {
            if (!pointInRect(x, y, layout.trajectoryDebugButtonRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }

            this->trajectoryDebugMask ^= trajectoryDebugBit(sliderIndex);
            this->sliderDragging = false;
            this->activeSliderIndex = -1;
            return true;
        }

        for (int sliderIndex = 0; sliderIndex < kTrajectorySliderCount; ++sliderIndex)
        {
            if (!pointInRect(x, y, layout.trajectoryRowRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }

            if (isArcSideControlIndex(sliderIndex))
            {
                const SDL_FRect& trackRect =
                    layout.trajectoryTrackRects[static_cast<size_t>(sliderIndex)];
                const SDL_FRect leftRect = buildArcSideChoiceRect(trackRect, false);
                const SDL_FRect rightRect = buildArcSideChoiceRect(trackRect, true);
                if (pointInRect(x, y, leftRect) || pointInRect(x, y, rightRect))
                {
                    MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning tuning =
                        MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
                            this->selectedDistanceBandIndex,
                            this->selectedAngleSectorIndex);
                    tuning.arcSide = pointInRect(x, y, leftRect) ? -1.0f : 1.0f;
                    MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
                        this->selectedDistanceBandIndex,
                        this->selectedAngleSectorIndex,
                        tuning);
                }

                this->sliderDragging = false;
                this->activeSliderIndex = -1;
                return true;
            }

            this->sliderDragging = true;
            this->activeSliderIndex = sliderIndex;
            this->sliderDragGrabOffsetX = 0.0f;

            MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning tuning =
                MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
                    this->selectedDistanceBandIndex,
                    this->selectedAngleSectorIndex);
            const float nextValue = trajectorySliderValueFromTrackPosition(
                sliderIndex,
                x,
                layout.trajectoryTrackRects[static_cast<size_t>(sliderIndex)]);
            setTrajectorySliderValue(&tuning, sliderIndex, nextValue);
            MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex,
                tuning);
            return true;
        }

        if (pointInRect(x, y, layout.headerRect))
        {
            this->panelDragging = true;
            this->panelDragGrabOffsetX = x - layout.panelRect.x;
            this->panelDragGrabOffsetY = y - layout.panelRect.y;
            return true;
        }

        return true;
    }

    if (this->activePageIndex == 2)
    {
        MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig config =
            MaritimeCannonSalvoSystem::getProjectileRibbonTrailConfig();

        if (layout.trailScrollMaxOffset > 1.0e-4f && pointInRect(x, y, layout.trailScrollThumbRect))
        {
            this->trailScrollDragging = true;
            this->trailScrollDragGrabOffsetY = y - layout.trailScrollThumbRect.y;
            return true;
        }
        if (layout.trailScrollMaxOffset > 1.0e-4f && pointInRect(x, y, layout.trailScrollTrackRect))
        {
            const float thumbTravel = (std::max)(0.0f, layout.trailScrollTrackRect.h - layout.trailScrollThumbRect.h);
            if (thumbTravel > 1.0e-4f)
            {
                const float targetThumbY = std::clamp(
                    y - (layout.trailScrollThumbRect.h * 0.5f),
                    layout.trailScrollTrackRect.y,
                    layout.trailScrollTrackRect.y + thumbTravel);
                const float scrollT = (targetThumbY - layout.trailScrollTrackRect.y) / thumbTravel;
                this->trailScrollOffsetY = scrollT * layout.trailScrollMaxOffset;
                this->trailScrollDragging = true;
                this->trailScrollDragGrabOffsetY = y - targetThumbY;
            }
            return true;
        }
        if (pointInRect(x, y, layout.trailEnableButtonRect))
        {
            const bool enable = !(config.meshEnabled || config.stampsEnabled);
            config.meshEnabled = enable;
            config.stampsEnabled = enable;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailMeshToggleButtonRect))
        {
            config.meshEnabled = !config.meshEnabled;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailStampsToggleButtonRect))
        {
            config.stampsEnabled = !config.stampsEnabled;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailBlendToggleButtonRect))
        {
            config.additiveStampBlend = !config.additiveStampBlend;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailManualModeButtonRect))
        {
            config.manualStampsEnabled = !config.manualStampsEnabled;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailManualAddButtonRect))
        {
            MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig::CustomStampPlacement stamp{};
            stamp.distanceFromHeadTiles = 0.0f;
            stamp.scale = 1.0f;
            stamp.opacity = 1.0f;
            config.customStamps.push_back(stamp);
            this->selectedManualTrailStampIndex = static_cast<int>(config.customStamps.size()) - 1;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailManualDuplicateButtonRect) &&
            this->selectedManualTrailStampIndex >= 0 &&
            this->selectedManualTrailStampIndex < static_cast<int>(config.customStamps.size()))
        {
            config.customStamps.push_back(config.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)]);
            this->selectedManualTrailStampIndex = static_cast<int>(config.customStamps.size()) - 1;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailManualDeleteButtonRect) &&
            this->selectedManualTrailStampIndex >= 0 &&
            this->selectedManualTrailStampIndex < static_cast<int>(config.customStamps.size()))
        {
            config.customStamps.erase(
                config.customStamps.begin() + static_cast<std::ptrdiff_t>(this->selectedManualTrailStampIndex));
            if (config.customStamps.empty())
            {
                this->selectedManualTrailStampIndex = -1;
            }
            else
            {
                this->selectedManualTrailStampIndex =
                    (std::min)(this->selectedManualTrailStampIndex, static_cast<int>(config.customStamps.size()) - 1);
            }
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailManualClearButtonRect))
        {
            config.customStamps.clear();
            this->selectedManualTrailStampIndex = -1;
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }
        if (pointInRect(x, y, layout.trailResetButtonRect))
        {
            MaritimeCannonSalvoSystem::resetProjectileRibbonTrailConfig();
            this->statusMessage = "Ribbon trail remis par defaut.";
            this->statusMessageTimerSec = 2.0f;
            this->selectedManualTrailStampIndex = -1;
            gEditorTrailClearSourceRequested = true;
            return true;
        }
        if (pointInRect(x, y, layout.trailImportButtonRect))
        {
            this->pendingDialogRequest = DialogRequest::TRAIL_IMPORT_JSON;
            return true;
        }
        if (pointInRect(x, y, layout.trailExportButtonRect))
        {
            this->pendingDialogRequest = DialogRequest::TRAIL_EXPORT_JSON;
            return true;
        }

        for (int sliderIndex = 0; sliderIndex < kTrailSliderCount; ++sliderIndex)
        {
            if (!pointInRect(x, y, layout.trailControlsViewportRect) ||
                !rectsIntersect(layout.trailRowRects[static_cast<size_t>(sliderIndex)], layout.trailControlsViewportRect) ||
                !pointInRect(x, y, layout.trailRowRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }

            this->sliderDragging = true;
            this->activeSliderIndex = sliderIndex;
            this->sliderDragGrabOffsetX = 0.0f;

            const float nextValue =
                trailSliderValueFromTrackPosition(
                    sliderIndex,
                    x,
                    layout.trailTrackRects[static_cast<size_t>(sliderIndex)]);
            setTrailSliderValue(&config, sliderIndex, nextValue);
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }

        for (int sliderIndex = 0; sliderIndex < kManualTrailSliderCount; ++sliderIndex)
        {
            if (!pointInRect(x, y, layout.trailManualSliderRowRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }
            if (this->selectedManualTrailStampIndex < 0 ||
                this->selectedManualTrailStampIndex >= static_cast<int>(config.customStamps.size()))
            {
                return true;
            }

            this->sliderDragging = true;
            this->activeSliderIndex = kManualTrailSliderIndexOffset + sliderIndex;
            this->sliderDragGrabOffsetX = 0.0f;
            const float nextValue = manualTrailSliderValueFromTrackPosition(
                sliderIndex,
                x,
                layout.trailManualSliderTrackRects[static_cast<size_t>(sliderIndex)]);
            setManualTrailSliderValue(
                &config.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)],
                sliderIndex,
                nextValue);
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }

        if (pointInRect(x, y, layout.trailManualPreviewRect))
        {
            const SDL_FRect viewport = SDL_FRect{
                layout.trailManualPreviewRect.x + 10.0f,
                layout.trailManualPreviewRect.y + 50.0f,
                layout.trailManualPreviewRect.w - 20.0f,
                layout.trailManualPreviewRect.h - 60.0f
            };
            const std::vector<TrailPreviewPoint> manualPoints =
                buildTrailPreviewPoints(viewport, config, this->previewAnimTimerSec, true);
            int nearestIndex = -1;
            float nearestDist2 = kManualStampSelectRadiusPx * kManualStampSelectRadiusPx;
            for (std::size_t index = 0U; index < config.customStamps.size(); ++index)
            {
                TrailPreviewSample sample{};
                if (!sampleTrailPreviewAtDistance(manualPoints, config, config.customStamps[index].distanceFromHeadTiles, &sample))
                {
                    continue;
                }
                const SDL_FPoint normal{-sample.tangent.y, sample.tangent.x};
                const float px = sample.position.x + (normal.x * config.customStamps[index].lateralOffsetPixels);
                const float py = sample.position.y + (normal.y * config.customStamps[index].lateralOffsetPixels);
                const float dx = x - px;
                const float dy = y - py;
                const float dist2 = (dx * dx) + (dy * dy);
                if (dist2 <= nearestDist2)
                {
                    nearestDist2 = dist2;
                    nearestIndex = static_cast<int>(index);
                }
            }

            if (nearestIndex >= 0)
            {
                this->selectedManualTrailStampIndex = nearestIndex;
                this->manualTrailStampDragActive = true;
                return true;
            }

            float bestDist = 0.0f;
            float bestDist2 = (std::numeric_limits<float>::max)();
            TrailPreviewSample bestSample{};
            bool bestValid = false;
            for (int step = 0; step <= 160; ++step)
            {
                const float d = -config.headCoverTiles +
                    (((config.maxLengthTiles + config.headCoverTiles) * static_cast<float>(step)) / 160.0f);
                TrailPreviewSample sample{};
                if (!sampleTrailPreviewAtDistance(manualPoints, config, d, &sample))
                {
                    continue;
                }
                const float dx = x - sample.position.x;
                const float dy = y - sample.position.y;
                const float dist2 = (dx * dx) + (dy * dy);
                if (dist2 < bestDist2)
                {
                    bestDist2 = dist2;
                    bestDist = d;
                    bestSample = sample;
                    bestValid = true;
                }
            }
            if (bestValid)
            {
                const SDL_FPoint normal{-bestSample.tangent.y, bestSample.tangent.x};
                MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig::CustomStampPlacement stamp{};
                stamp.distanceFromHeadTiles = bestDist;
                stamp.lateralOffsetPixels =
                    ((x - bestSample.position.x) * normal.x) + ((y - bestSample.position.y) * normal.y);
                stamp.scale = 1.0f;
                stamp.opacity = 1.0f;
                config.customStamps.push_back(stamp);
                this->selectedManualTrailStampIndex = static_cast<int>(config.customStamps.size()) - 1;
                this->manualTrailStampDragActive = true;
                MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            }
            return true;
        }

        if (pointInRect(x, y, layout.headerRect))
        {
            this->panelDragging = true;
            this->panelDragGrabOffsetX = x - layout.panelRect.x;
            this->panelDragGrabOffsetY = y - layout.panelRect.y;
            return true;
        }

        return true;
    }

    if (pointInRect(x, y, layout.resetButtonRect))
    {
        VFXClassic::IlluminatedProjectileGlowConfig defaultGlowConfig{};
        const bool loadedDefaultGlow = VFXClassic::readIlluminatedProjectileGlowConfigFromFile(
            "assets/data/ammo-illu-default.json",
            &defaultGlowConfig);
        if (loadedDefaultGlow)
        {
            VFXClassic::setIlluminatedProjectileGlowConfig(defaultGlowConfig);
            this->statusMessage = "Glow remis depuis ammo-illu-default.json.";
            gEditorGlowLiveClearSourceRequested = true;
        }
        else
        {
            VFXClassic::resetIlluminatedProjectileGlowConfig();
            this->statusMessage = "Glow remis par defaut interne (json introuvable).";
            gEditorGlowLiveClearSourceRequested = true;
        }
        this->statusMessageTimerSec = 2.0f;
        return true;
    }

    if (pointInRect(x, y, layout.glowImportButtonRect))
    {
        this->pendingDialogRequest = DialogRequest::GLOW_IMPORT_JSON;
        return true;
    }

    if (pointInRect(x, y, layout.glowExportButtonRect))
    {
        this->pendingDialogRequest = DialogRequest::GLOW_EXPORT_JSON;
        return true;
    }

    VFXClassic::IlluminatedProjectileGlowConfig config =
        VFXClassic::getIlluminatedProjectileGlowConfig();
    for (int layerIndex = 0; layerIndex < kLayerButtonCount; ++layerIndex)
    {
        if (!pointInRect(x, y, layout.layerButtonRects[static_cast<size_t>(layerIndex)]))
        {
            continue;
        }

        setLayerEnabled(&config, layerIndex, !getLayerEnabled(config, layerIndex));
        VFXClassic::setIlluminatedProjectileGlowConfig(config);
        return true;
    }

    if (pointInRect(x, y, layout.compactToggleRect))
    {
        setCompactGlowEnabled(&config, !getCompactGlowEnabled(config));
        VFXClassic::setIlluminatedProjectileGlowConfig(config);
        return true;
    }

    for (int sliderIndex = 0; sliderIndex < kSliderCount; ++sliderIndex)
    {
        if (!pointInRect(x, y, layout.rowRects[static_cast<size_t>(sliderIndex)]))
        {
            continue;
        }

        this->sliderDragging = true;
        this->activeSliderIndex = sliderIndex;
        this->sliderDragGrabOffsetX = 0.0f;

        const float nextValue =
            sliderValueFromTrackPosition(sliderIndex, x, layout.trackRects[static_cast<size_t>(sliderIndex)]);
        setSliderValue(&config, sliderIndex, nextValue);
        VFXClassic::setIlluminatedProjectileGlowConfig(config);
        return true;
    }

    if (pointInRect(x, y, layout.headerRect))
    {
        this->panelDragging = true;
        this->panelDragGrabOffsetX = x - layout.panelRect.x;
        this->panelDragGrabOffsetY = y - layout.panelRect.y;
        return true;
    }

    return true;
}

bool IlluminatedProjectileDebugPanel::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)x;
    (void)y;
    (void)integer_x;
    (void)mouseID;

    if (!this->loaded || !this->visible)
    {
        return false;
    }

    const IlluminatedProjectileDebugPanelLayout layout = buildLayout(this->panelOffset, this->trailScrollOffsetY);
    if (!pointInRect(mouse_x, mouse_y, layout.panelRect))
    {
        return false;
    }

    const int delta = resolveWheelDelta(direction, y, integer_y);
    if (delta == 0)
    {
        return true;
    }

    if (this->activePageIndex == 1)
    {
        if (pointInRect(mouse_x, mouse_y, layout.shipSpeedRowRect))
        {
            const float currentValue = getDebugShipSpeedTilesPerSecond();
            const float nextValue = std::clamp(
                currentValue + (static_cast<float>(delta) * kShipSpeedSliderSpec.wheelStep),
                kShipSpeedSliderSpec.minValue,
                kShipSpeedSliderSpec.maxValue);
            setDebugShipSpeedTilesPerSecond(nextValue);
            return true;
        }

        for (int sliderIndex = 0; sliderIndex < kTrajectorySliderCount; ++sliderIndex)
        {
            if (!pointInRect(mouse_x, mouse_y, layout.trajectoryRowRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }

            MaritimeCannonSalvoSystem::ProjectileTrajectoryTuning tuning =
                MaritimeCannonSalvoSystem::getProjectileTrajectoryTuning(
                    this->selectedDistanceBandIndex,
                    this->selectedAngleSectorIndex);
            if (isArcSideControlIndex(sliderIndex))
            {
                tuning.arcSide = (delta < 0) ? -1.0f : 1.0f;
                MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
                    this->selectedDistanceBandIndex,
                    this->selectedAngleSectorIndex,
                    tuning);
                return true;
            }

            const SliderSpec& spec = kTrajectorySliderSpecs[static_cast<size_t>(sliderIndex)];
            const float currentValue = getTrajectorySliderValue(tuning, sliderIndex);
            const float nextValue =
                std::clamp(currentValue + (static_cast<float>(delta) * spec.wheelStep), spec.minValue, spec.maxValue);
            setTrajectorySliderValue(&tuning, sliderIndex, nextValue);
            MaritimeCannonSalvoSystem::setProjectileTrajectoryTuning(
                this->selectedDistanceBandIndex,
                this->selectedAngleSectorIndex,
                tuning);
            return true;
        }

        return true;
    }

    if (this->activePageIndex == 2)
    {
        MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig config =
            MaritimeCannonSalvoSystem::getProjectileRibbonTrailConfig();
        for (int sliderIndex = 0; sliderIndex < kTrailSliderCount; ++sliderIndex)
        {
            if (!pointInRect(mouse_x, mouse_y, layout.trailControlsViewportRect) ||
                !rectsIntersect(layout.trailRowRects[static_cast<size_t>(sliderIndex)], layout.trailControlsViewportRect) ||
                !pointInRect(mouse_x, mouse_y, layout.trailRowRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }

            const SliderSpec& spec = kTrailSliderSpecs[static_cast<size_t>(sliderIndex)];
            const float currentValue = getTrailSliderValue(config, sliderIndex);
            const float nextValue =
                std::clamp(currentValue + (static_cast<float>(delta) * spec.wheelStep), spec.minValue, spec.maxValue);
            setTrailSliderValue(&config, sliderIndex, nextValue);
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }

        for (int sliderIndex = 0; sliderIndex < kManualTrailSliderCount; ++sliderIndex)
        {
            if (!pointInRect(mouse_x, mouse_y, layout.trailManualSliderRowRects[static_cast<size_t>(sliderIndex)]))
            {
                continue;
            }
            if (this->selectedManualTrailStampIndex < 0 ||
                this->selectedManualTrailStampIndex >= static_cast<int>(config.customStamps.size()))
            {
                return true;
            }

            const SliderSpec& spec = kManualTrailSliderSpecs[static_cast<size_t>(sliderIndex)];
            const float currentValue = getManualTrailSliderValue(
                config.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)],
                sliderIndex);
            const float nextValue =
                std::clamp(currentValue + (static_cast<float>(delta) * spec.wheelStep), spec.minValue, spec.maxValue);
            setManualTrailSliderValue(
                &config.customStamps[static_cast<size_t>(this->selectedManualTrailStampIndex)],
                sliderIndex,
                nextValue);
            MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(config);
            return true;
        }

        if (pointInRect(mouse_x, mouse_y, layout.panelRect) &&
            mouse_x < layout.trailPreviewRect.x)
        {
            const float scrollStep = (kSliderRowHeight + kSliderRowGap) * 0.8f;
            this->trailScrollOffsetY = std::clamp(
                this->trailScrollOffsetY - (static_cast<float>(delta) * scrollStep),
                0.0f,
                layout.trailScrollMaxOffset);
            return true;
        }

        return true;
    }

    for (int sliderIndex = 0; sliderIndex < kSliderCount; ++sliderIndex)
    {
        if (!pointInRect(mouse_x, mouse_y, layout.rowRects[static_cast<size_t>(sliderIndex)]))
        {
            continue;
        }

        VFXClassic::IlluminatedProjectileGlowConfig config =
            VFXClassic::getIlluminatedProjectileGlowConfig();
        const SliderSpec& spec = kSliderSpecs[static_cast<size_t>(sliderIndex)];
        const float currentValue = getSliderValue(config, sliderIndex);
        const float nextValue =
            std::clamp(currentValue + (static_cast<float>(delta) * spec.wheelStep), spec.minValue, spec.maxValue);
        setSliderValue(&config, sliderIndex, nextValue);
        VFXClassic::setIlluminatedProjectileGlowConfig(config);
        return true;
    }

    return true;
}

void IlluminatedProjectileDebugPanel::toggleVisibility(void)
{
    this->setVisible(!this->visible);
}

void IlluminatedProjectileDebugPanel::setVisible(bool visibleState)
{
    this->visible = visibleState;
    if (!this->visible)
    {
        this->panelDragging = false;
        this->sliderDragging = false;
        this->trailScrollDragging = false;
        this->activeSliderIndex = -1;
    }
}

bool IlluminatedProjectileDebugPanel::isVisible(void) const
{
    return this->visible;
}

IlluminatedProjectileDebugPanel::DialogRequest IlluminatedProjectileDebugPanel::consumeDialogRequest(void)
{
    const DialogRequest request = this->pendingDialogRequest;
    this->pendingDialogRequest = DialogRequest::NONE;
    return request;
}


namespace
{
struct OceanColorEntry
{
    OceanShader::WaterColor value;
    const char* label;
};

constexpr int kDefaultSalvoBallCount = 10;
constexpr float kDefaultSalvoIntervalSec = 4.0f;
constexpr float kListRowHeight = 20.0f;
constexpr float kListRowGap = 3.0f;
constexpr float kListPadding = 5.0f;
constexpr float kListHeaderHeight = 18.0f;
constexpr float kListImportButtonHeight = 24.0f;
constexpr float kListScrollBarWidth = 10.0f;
constexpr float kRightPanelWidth = 318.0f;
constexpr float kRightPanelGap = 8.0f;
constexpr float kTopButtonHeight = 28.0f;
constexpr float kEditorZoomMin = 0.40f;
constexpr float kEditorZoomMax = 1.40f;
constexpr float kMiniMapVisualScale = 2.0f / 3.0f;

constexpr std::array<OceanColorEntry, 27> kOceanColors = {{
    {OceanShader::WaterColor::BLUE, "BLUE"},
    {OceanShader::WaterColor::AMBER, "AMBER"},
    {OceanShader::WaterColor::BROWN, "BROWN"},
    {OceanShader::WaterColor::CORAL, "CORAL"},
    {OceanShader::WaterColor::CYAN, "CYAN"},
    {OceanShader::WaterColor::GREEN, "GREEN"},
    {OceanShader::WaterColor::JADE, "JADE"},
    {OceanShader::WaterColor::LAVENDER, "LAVENDER"},
    {OceanShader::WaterColor::LIME, "LIME"},
    {OceanShader::WaterColor::MAGENTA, "MAGENTA"},
    {OceanShader::WaterColor::MINT, "MINT"},
    {OceanShader::WaterColor::OBSIDIAN, "OBSIDIAN"},
    {OceanShader::WaterColor::ORANGE, "ORANGE"},
    {OceanShader::WaterColor::PEACH, "PEACH"},
    {OceanShader::WaterColor::PINK, "PINK"},
    {OceanShader::WaterColor::PLUM, "PLUM"},
    {OceanShader::WaterColor::PURPLE, "PURPLE"},
    {OceanShader::WaterColor::RED, "RED"},
    {OceanShader::WaterColor::ROSE, "ROSE"},
    {OceanShader::WaterColor::SEAWEED, "SEAWEED"},
    {OceanShader::WaterColor::SLATE, "SLATE"},
    {OceanShader::WaterColor::STORM, "STORM"},
    {OceanShader::WaterColor::SUNSET, "SUNSET"},
    {OceanShader::WaterColor::TEAL, "TEAL"},
    {OceanShader::WaterColor::TURQUOISE, "TURQUOISE"},
    {OceanShader::WaterColor::VIOLET, "VIOLET"},
    {OceanShader::WaterColor::YELLOW, "YELLOW"},
}};

constexpr RC2D_Color kHudTextColor = RC2D_Color{235, 242, 250, 250};
constexpr RC2D_Color kHudMutedTextColor = RC2D_Color{188, 200, 214, 230};
constexpr RC2D_Color kHudStatusColor = RC2D_Color{230, 200, 90, 250};
constexpr RC2D_Color kButtonFillColor = RC2D_Color{36, 44, 52, 210};
constexpr RC2D_Color kButtonFillActiveColor = RC2D_Color{83, 122, 92, 230};
constexpr RC2D_Color kButtonBorderColor = RC2D_Color{140, 150, 165, 230};
constexpr RC2D_Color kButtonBorderActiveColor = RC2D_Color{205, 226, 190, 245};
constexpr RC2D_Color kPanelFillColor = RC2D_Color{13, 20, 30, 218};
constexpr RC2D_Color kPanelBorderColor = RC2D_Color{112, 130, 150, 230};
constexpr RC2D_Color kRowFillColor = RC2D_Color{27, 39, 52, 218};
constexpr RC2D_Color kRowSelectedFillColor = RC2D_Color{73, 105, 137, 236};
constexpr RC2D_Color kRowMultiSelectedFillColor = RC2D_Color{88, 118, 82, 236};
constexpr RC2D_Color kRowFocusedBorderColor = RC2D_Color{226, 210, 145, 245};

constexpr std::array<RC2D_FileDialogFilter, 1> kFolderFilters = {{
    {"Dossier", "*"},
}};

constexpr std::array<RC2D_FileDialogFilter, 1> kJsonFileFilters = {{
    {"JSON", "json"},
}};

Ship* gActiveCannonSalvoAttackerShip = nullptr;
Ship* gActiveCannonSalvoTargetShip = nullptr;
bool gEditorGlowModeDefault = false;
std::string gEditorGlowLiveSourcePath{};
bool gEditorGlowLiveClearSourceRequested = false;
std::string gEditorTrailSourcePath{};
bool gEditorTrailClearSourceRequested = false;

static std::string normalizePathSlashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static bool startsWith(const std::string& value, const char* prefix)
{
    return prefix != nullptr && value.rfind(prefix, 0U) == 0U;
}

static float computeEditorMiniMapSize(const SDL_FRect& mapRect)
{
    return (std::clamp)(
        mapRect.h * 0.22f * kMiniMapVisualScale,
        120.0f * kMiniMapVisualScale,
        190.0f * kMiniMapVisualScale);
}

static float miniMapNormalizedToSectorCenter(float normalizedValue, int sectorCount)
{
    const float span = static_cast<float>((std::max)(sectorCount, 1));
    const float maxSectorCenter = static_cast<float>((std::max)(sectorCount - 1, 0));
    return (std::clamp)((normalizedValue * span) - 0.5f, 0.0f, maxSectorCenter);
}

static bool isAnyEditorCameraScancodeDown(std::initializer_list<SDL_Scancode> scancodes)
{
    for (SDL_Scancode scancode : scancodes)
    {
        if (scancode != SDL_SCANCODE_UNKNOWN &&
            rc2d_keyboard_isScancodeDown(static_cast<RC2D_Scancode>(scancode)))
        {
            return true;
        }
    }

    return false;
}

static bool updateEditorCameraKeyboardScroll(
    double dt,
    Camera& camera,
    const Map& map,
    const SDL_FRect& viewportRect)
{
    const bool upPressed =
        isAnyEditorCameraScancodeDown({SDL_SCANCODE_UP, SDL_SCANCODE_Z, SDL_SCANCODE_W});
    const bool downPressed =
        isAnyEditorCameraScancodeDown({SDL_SCANCODE_DOWN, SDL_SCANCODE_S});
    const bool leftPressed =
        isAnyEditorCameraScancodeDown({SDL_SCANCODE_LEFT, SDL_SCANCODE_Q, SDL_SCANCODE_A});
    const bool rightPressed =
        isAnyEditorCameraScancodeDown({SDL_SCANCODE_RIGHT, SDL_SCANCODE_D});

    float deltaSectorX = 0.0f;
    float deltaSectorY = 0.0f;

    if (upPressed && !downPressed)
    {
        deltaSectorY -= 1.0f;
    }
    else if (downPressed && !upPressed)
    {
        deltaSectorY += 1.0f;
    }

    if (leftPressed && !rightPressed)
    {
        deltaSectorX -= 1.0f;
    }
    else if (rightPressed && !leftPressed)
    {
        deltaSectorX += 1.0f;
    }

    if (deltaSectorX == 0.0f && deltaSectorY == 0.0f)
    {
        return false;
    }

    if (deltaSectorX != 0.0f && deltaSectorY != 0.0f)
    {
        deltaSectorX *= Camera::CAMERA_DIAGONAL_FACTOR;
        deltaSectorY *= Camera::CAMERA_DIAGONAL_FACTOR;
    }

    const float sectorDistance =
        (std::max)(0.0f, Camera::CAMERA_SCROLL_SPEED_SECTORS) * static_cast<float>(dt);
    deltaSectorX *= sectorDistance;
    deltaSectorY *= sectorDistance;

    const float deltaTileX =
        (deltaSectorX + deltaSectorY) * static_cast<float>(Map::SECTOR_STEP);
    const float deltaTileY =
        (deltaSectorY - deltaSectorX) * static_cast<float>(Map::SECTOR_STEP);

    camera.moveCameraTiles(deltaTileX, deltaTileY, map, viewportRect);
    return true;
}

static std::string folderDisplayName(const std::string& path)
{
    std::filesystem::path fsPath(path);
    std::string name = fsPath.filename().generic_string();
    if (name.empty())
    {
        name = path;
    }
    return name;
}

static std::string shortenMiddle(const std::string& text, std::size_t maxLen)
{
    if (text.size() <= maxLen || maxLen < 8U)
    {
        return text;
    }

    const std::size_t headLen = maxLen / 2U;
    const std::size_t tailLen = maxLen - headLen - 3U;
    return text.substr(0U, headLen) + "..." + text.substr(text.size() - tailLen);
}

static bool directoryContainsFile(const std::filesystem::path& folder, const char* fileName)
{
    std::error_code fsError;
    return std::filesystem::exists(folder / fileName, fsError);
}

static bool isValidShipFolder(const std::filesystem::path& folder)
{
    std::error_code fsError;
    if (!std::filesystem::is_directory(folder, fsError))
    {
        return false;
    }

    for (int i = 1; i <= 8; ++i)
    {
        const std::string spriteName = std::to_string(i) + ".png";
        if (!directoryContainsFile(folder, spriteName.c_str()))
        {
            return false;
        }
    }
    return true;
}

static bool isLikelyVfxClassicFolder(const std::filesystem::path& folder)
{
    std::error_code fsError;
    if (!std::filesystem::is_directory(folder, fsError))
    {
        return false;
    }

    const std::string name = folder.filename().generic_string();
    return directoryContainsFile(folder, (name + "-spritesheet.json").c_str());
}

static bool isLikelyVfxShipFolder(const std::filesystem::path& folder)
{
    std::error_code fsError;
    if (!std::filesystem::is_directory(folder, fsError))
    {
        return false;
    }

    const std::string name = folder.filename().generic_string();
    return directoryContainsFile(folder, (name + "-spritesheet.json").c_str());
}

static std::string makeRelativeProjectPath(const std::string& inputPath)
{
    std::error_code fsError;
    std::filesystem::path absolute = std::filesystem::absolute(std::filesystem::path(inputPath), fsError);
    if (fsError)
    {
        return normalizePathSlashes(inputPath);
    }

    std::filesystem::path relative = std::filesystem::relative(absolute, std::filesystem::current_path(), fsError);
    const std::string relativeText = normalizePathSlashes(relative.generic_string());
    if (!fsError && !relative.empty() && relativeText.rfind("..", 0U) != 0U)
    {
        return relativeText;
    }

    return normalizePathSlashes(absolute.generic_string());
}

static void pushUniqueFolder(std::vector<EditorMapCannonSalvoScene::ListItem>* items, const std::string& folderPath)
{
    if (items == nullptr || folderPath.empty())
    {
        return;
    }

    const std::string normalized = normalizePathSlashes(folderPath);
    const auto exists = std::find_if(items->begin(), items->end(), [&](const EditorMapCannonSalvoScene::ListItem& item) {
        return item.folderPath == normalized;
    });
    if (exists != items->end())
    {
        return;
    }

    EditorMapCannonSalvoScene::ListItem item{};
    item.folderPath = normalized;
    item.displayName = folderDisplayName(normalized);
    items->push_back(std::move(item));
}

static const char* projectileGlowModeLabel(EditorMapCannonSalvoScene::ProjectileGlowMode mode)
{
    switch (mode)
    {
        case EditorMapCannonSalvoScene::ProjectileGlowMode::DEFAULT_JSON:
            return "MODE GLOW DEFAULT";
        case EditorMapCannonSalvoScene::ProjectileGlowMode::LIVE:
        default:
            return "MODE GLOW LIVE";
    }
}

static bool tryGetTrajectoryPreviewShips(SDL_FPoint* outAttackerTile, SDL_FPoint* outTargetTile)
{
    if (outAttackerTile == nullptr || outTargetTile == nullptr)
    {
        return false;
    }

    if (gActiveCannonSalvoAttackerShip != nullptr &&
        gActiveCannonSalvoTargetShip != nullptr &&
        gActiveCannonSalvoAttackerShip->areSpritesLoaded() &&
        gActiveCannonSalvoTargetShip->areSpritesLoaded())
    {
        *outAttackerTile = gActiveCannonSalvoAttackerShip->getPositionTile();
        *outTargetTile = gActiveCannonSalvoTargetShip->getPositionTile();
        return true;
    }

    GameState& gameState = GetGameState();
    if (gameState.otherPlayers.empty())
    {
        return false;
    }

    *outAttackerTile = gameState.player.getShip().getPositionTile();
    *outTargetTile = gameState.otherPlayers[0].getShip().getPositionTile();
    return true;
}
} // namespace

EditorMapCannonSalvoScene::EditorMapCannonSalvoScene(void)
    : backgroundWidget{},
      scrollBarOverlay{},
      clickMarker{},
      illuminatedProjectileDebugPanel{},
      overlayFont{},
      attackerShip{},
      targetShip{},
      localVfxShips{},
      shipOptions{},
      projectileOptions{},
      trailOptions{},
      startVfxOptions{},
      endVfxOptions{},
      selectedShipIndex(-1),
      selectedProjectileIndex(-1),
      selectedTrailIndex(-1),
      selectedStartVfxIndex(-1),
      focusedEndVfxIndex(-1),
      shipListScrollOffset(0),
      projectileListScrollOffset(0),
      trailListScrollOffset(0),
      startVfxListScrollOffset(0),
      endVfxListScrollOffset(0),
      shipListScrollDragActive(false),
      projectileListScrollDragActive(false),
      trailListScrollDragActive(false),
      startVfxListScrollDragActive(false),
      endVfxListScrollDragActive(false),
      shipListScrollDragGrabOffsetY(0.0f),
      projectileListScrollDragGrabOffsetY(0.0f),
      trailListScrollDragGrabOffsetY(0.0f),
      startVfxListScrollDragGrabOffsetY(0.0f),
      endVfxListScrollDragGrabOffsetY(0.0f),
      selectedOceanColorIndex(24),
      pendingOceanColorDelta(0),
      selectedSalvoBallCount(kDefaultSalvoBallCount),
      salvoIntervalSec(kDefaultSalvoIntervalSec),
      salvoTimerSec(0.0f),
      lastObservedGlowConfigValid(false),
      lastObservedGlowConfig{},
      showLists(true),
      editorTextInputEnabled(false),
      endDelayInputFocused(false),
      endDelayInputBuffer("0"),
      statusMessage("Editeur salves maritime pret."),
      blockingPopupVisible(false),
      blockingPopupMessage{},
      buttonDebugPanelRect{},
      buttonListsVisibilityRect{},
      buttonOceanPrevRect{},
      buttonOceanNextRect{},
      buttonCenterAttackerRect{},
      buttonZoomOutRect{},
      buttonZoomInRect{},
      buttonSalvoOneRect{},
      buttonSalvoFiveRect{},
      buttonSalvoTenRect{},
      buttonShipSpeedDownRect{},
      buttonShipSpeedUpRect{},
      buttonCadenceDownRect{},
      buttonCadenceUpRect{},
      buttonProjectileIlluminatedRect{},
      buttonProjectileTrailRect{},
      buttonProjectileGlowModeRect{},
      buttonProjectileGlowImportRect{},
      buttonProjectileGlowExportRect{},
      endDelayInputRect{},
      shipListRect{},
      projectileListRect{},
      trailListRect{},
      startVfxListRect{},
      endVfxListRect{},
      projectileImportButtonRect{},
      trailImportButtonRect{},
      startVfxImportButtonRect{},
      endVfxImportButtonRect{},
      miniMapRect{},
      miniMapDragActive(false),
      miniMapDragOffsetX(0.0f),
      miniMapDragOffsetY(0.0f),
      pendingFolderDialogCompleted(false),
      pendingFolderDialogCanceled(false),
      pendingImportTarget(ImportTarget::NONE),
      pendingFolderAbsolute{},
      pendingFolderMutex{}
{
}

EditorMapCannonSalvoScene::~EditorMapCannonSalvoScene(void)
{
}

void EditorMapCannonSalvoScene::resetEditorState(void)
{
    this->selectedShipIndex = -1;
    this->selectedProjectileIndex = -1;
    this->selectedTrailIndex = -1;
    this->selectedStartVfxIndex = -1;
    this->focusedEndVfxIndex = -1;
    this->shipListScrollOffset = 0;
    this->projectileListScrollOffset = 0;
    this->trailListScrollOffset = 0;
    this->startVfxListScrollOffset = 0;
    this->endVfxListScrollOffset = 0;
    this->shipListScrollDragActive = false;
    this->projectileListScrollDragActive = false;
    this->trailListScrollDragActive = false;
    this->startVfxListScrollDragActive = false;
    this->endVfxListScrollDragActive = false;
    this->shipListScrollDragGrabOffsetY = 0.0f;
    this->projectileListScrollDragGrabOffsetY = 0.0f;
    this->trailListScrollDragGrabOffsetY = 0.0f;
    this->startVfxListScrollDragGrabOffsetY = 0.0f;
    this->endVfxListScrollDragGrabOffsetY = 0.0f;
    this->selectedOceanColorIndex = 24;
    this->pendingOceanColorDelta = 0;
    this->selectedSalvoBallCount = kDefaultSalvoBallCount;
    this->salvoIntervalSec = kDefaultSalvoIntervalSec;
    this->salvoTimerSec = 0.0f;
    this->lastObservedGlowConfigValid = false;
    this->lastObservedGlowConfig = VFXClassic::getDefaultIlluminatedProjectileGlowConfig();
    this->showLists = true;
    this->endDelayInputFocused = false;
    this->endDelayInputBuffer = "0";
    this->statusMessage = "Editeur salves maritime pret.";
    this->blockingPopupVisible = false;
    this->blockingPopupMessage.clear();
    this->buttonDebugPanelRect = SDL_FRect{};
    this->buttonListsVisibilityRect = SDL_FRect{};
    this->buttonOceanPrevRect = SDL_FRect{};
    this->buttonOceanNextRect = SDL_FRect{};
    this->buttonCenterAttackerRect = SDL_FRect{};
    this->buttonZoomOutRect = SDL_FRect{};
    this->buttonZoomInRect = SDL_FRect{};
    this->buttonSalvoOneRect = SDL_FRect{};
    this->buttonSalvoFiveRect = SDL_FRect{};
    this->buttonSalvoTenRect = SDL_FRect{};
    this->buttonShipSpeedDownRect = SDL_FRect{};
    this->buttonShipSpeedUpRect = SDL_FRect{};
    this->buttonCadenceDownRect = SDL_FRect{};
    this->buttonCadenceUpRect = SDL_FRect{};
    this->buttonProjectileIlluminatedRect = SDL_FRect{};
    this->buttonProjectileTrailRect = SDL_FRect{};
    this->buttonProjectileGlowModeRect = SDL_FRect{};
    this->buttonProjectileGlowImportRect = SDL_FRect{};
    this->buttonProjectileGlowExportRect = SDL_FRect{};
    this->endDelayInputRect = SDL_FRect{};
    this->shipListRect = SDL_FRect{};
    this->projectileListRect = SDL_FRect{};
    this->trailListRect = SDL_FRect{};
    this->startVfxListRect = SDL_FRect{};
    this->endVfxListRect = SDL_FRect{};
    this->projectileImportButtonRect = SDL_FRect{};
    this->trailImportButtonRect = SDL_FRect{};
    this->startVfxImportButtonRect = SDL_FRect{};
    this->endVfxImportButtonRect = SDL_FRect{};
    this->miniMapRect = SDL_FRect{};
    this->miniMapDragActive = false;
    this->miniMapDragOffsetX = 0.0f;
    this->miniMapDragOffsetY = 0.0f;
    this->pendingImportTarget = ImportTarget::NONE;
    this->pendingFolderAbsolute.clear();
    this->clickMarker.setDurationSeconds(0.85);
    this->clickMarker.hide();
    this->syncEditorTextInputState();
}

void EditorMapCannonSalvoScene::clearLocalVfxShips(void)
{
    for (GameplayVfxShipSlot& slot : this->localVfxShips)
    {
        slot.vfx.unload();
    }
    this->localVfxShips.clear();
}

void EditorMapCannonSalvoScene::collectAssetLists(void)
{
    this->shipOptions.clear();
    this->projectileOptions.clear();
    this->trailOptions.clear();
    this->startVfxOptions.clear();
    this->endVfxOptions.clear();

    this->collectShipFolders();
    this->collectProjectileFolders();
    this->collectTrailFolders();
    this->collectShipVfxFolders();

    auto selectByFolderName = [](const std::vector<ListItem>& items, const char* folderName) -> int {
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            if (items[static_cast<std::size_t>(i)].displayName == folderName)
            {
                return i;
            }
        }
        return -1;
    };

    this->selectedShipIndex = selectByFolderName(this->shipOptions, "bateau elite 10");
    if (this->selectedShipIndex < 0 && !this->shipOptions.empty())
    {
        this->selectedShipIndex = 0;
    }

    this->selectedProjectileIndex = selectByFolderName(this->projectileOptions, "vfx-ammo-explo");
    if (this->selectedProjectileIndex < 0 && !this->projectileOptions.empty())
    {
        this->selectedProjectileIndex = 0;
    }

    this->selectedTrailIndex = selectByFolderName(this->trailOptions, "vfx-ammo-rep");
    if (this->selectedTrailIndex < 0)
    {
        this->selectedTrailIndex = selectByFolderName(this->trailOptions, "vfx-ammo-illu");
    }
    if (this->selectedTrailIndex < 0 && !this->trailOptions.empty())
    {
        this->selectedTrailIndex = 0;
    }

    this->selectedStartVfxIndex = selectByFolderName(this->startVfxOptions, "vfx-cannon");

    const int defaultEnd = selectByFolderName(this->endVfxOptions, "vfx-hitsimple");
    if (defaultEnd >= 0)
    {
        this->endVfxOptions[static_cast<std::size_t>(defaultEnd)].selected = true;
        this->focusedEndVfxIndex = defaultEnd;
    }
}

void EditorMapCannonSalvoScene::collectShipFolders(void)
{
    std::error_code fsError;
    const std::filesystem::path root("assets/images/ships");
    if (!std::filesystem::is_directory(root, fsError))
    {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, fsError))
    {
        if (fsError || !entry.is_directory(fsError) || !isValidShipFolder(entry.path()))
        {
            continue;
        }
        pushUniqueFolder(&this->shipOptions, entry.path().generic_string());
    }
}

void EditorMapCannonSalvoScene::collectProjectileFolders(void)
{
    std::error_code fsError;
    const std::filesystem::path root("assets/images/vfxclassic");
    if (!std::filesystem::is_directory(root, fsError))
    {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, fsError))
    {
        const std::string folderName = entry.path().filename().generic_string();
        if (fsError || !entry.is_directory(fsError) || !startsWith(folderName, "vfx-ammo-") ||
            !isLikelyVfxClassicFolder(entry.path()))
        {
            continue;
        }
        pushUniqueFolder(&this->projectileOptions, entry.path().generic_string());
    }
}

void IlluminatedProjectileDebugPanel::setTrailPreviewSelection(
    const char* projectileFolderPath,
    const char* trailFolderPath,
    bool projectileIlluminated,
    bool trailEnabled)
{
    this->previewProjectileFolderPath =
        (projectileFolderPath != nullptr) ? projectileFolderPath : "";
    this->previewTrailFolderPath =
        (trailFolderPath != nullptr) ? trailFolderPath : "";
    this->previewProjectileIlluminated = projectileIlluminated;
    this->previewTrailEnabled = trailEnabled;
    this->previewAssetsDirty = true;
}

void EditorMapCannonSalvoScene::collectTrailFolders(void)
{
    std::error_code fsError;
    const std::filesystem::path root("assets/images/vfxclassic");
    if (!std::filesystem::is_directory(root, fsError))
    {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, fsError))
    {
        if (fsError || !entry.is_directory(fsError) || !isLikelyVfxClassicFolder(entry.path()))
        {
            continue;
        }
        pushUniqueFolder(&this->trailOptions, entry.path().generic_string());
    }
}

void EditorMapCannonSalvoScene::collectShipVfxFolders(void)
{
    std::error_code fsError;
    const std::filesystem::path root("assets/images/vfxship");
    if (!std::filesystem::is_directory(root, fsError))
    {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, fsError))
    {
        const std::string folderName = entry.path().filename().generic_string();
        if (fsError || !entry.is_directory(fsError) || startsWith(folderName, "vfx-ammo-") ||
            !isLikelyVfxShipFolder(entry.path()))
        {
            continue;
        }
        pushUniqueFolder(&this->startVfxOptions, entry.path().generic_string());
        pushUniqueFolder(&this->endVfxOptions, entry.path().generic_string());
    }
}

bool EditorMapCannonSalvoScene::selectShipAtIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->shipOptions.size()))
    {
        return false;
    }

    const SDL_FPoint oldAttacker = this->attackerShip.getPositionTile();
    const SDL_FPoint oldTarget = this->targetShip.getPositionTile();
    const bool hadPositions = this->attackerShip.areSpritesLoaded() || this->targetShip.areSpritesLoaded();
    const float currentSpeed =
        this->attackerShip.areSpritesLoaded()
            ? this->attackerShip.getSpeedTilesPerSecond()
            : 4.2f;
    const std::string folder = this->shipOptions[static_cast<std::size_t>(index)].folderPath;

    this->attackerShip.unloadSprites();
    this->targetShip.unloadSprites();
    if (!this->attackerShip.loadSpritesFromFolder(folder.c_str()) ||
        !this->targetShip.loadSpritesFromFolder(folder.c_str()))
    {
        this->statusMessage = "Impossible de charger le navire: " + folder;
        return false;
    }

    this->selectedShipIndex = index;
    this->attackerShip.setSpeedTilesPerSecond(currentSpeed);
    this->targetShip.setSpeedTilesPerSecond(currentSpeed);
    this->attackerShip.setDrawScale(1.0f);
    this->targetShip.setDrawScale(1.0f);
    if (hadPositions)
    {
        this->attackerShip.setPositionTile(oldAttacker.x, oldAttacker.y);
        this->targetShip.setPositionTile(oldTarget.x, oldTarget.y);
    }
    else
    {
        this->positionShipsForPreview(false);
    }
    this->statusMessage = "Navires: " + this->shipOptions[static_cast<std::size_t>(index)].displayName;
    return true;
}

bool EditorMapCannonSalvoScene::selectProjectileAtIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->projectileOptions.size()))
    {
        return false;
    }

    (void)this->syncSelectedProjectileGlowConfigFromPanel();
    (void)this->syncSelectedProjectileRibbonTrailConfigFromPanel();
    this->selectedProjectileIndex = index;
    ListItem& selectedItem = this->projectileOptions[static_cast<std::size_t>(index)];
    if (selectedItem.projectileGlowMode == ProjectileGlowMode::LIVE &&
        !selectedItem.illuminatedGlowConfigInitialized)
    {
        VFXClassic::IlluminatedProjectileGlowConfig defaultGlowConfig{};
        const bool loadedDefaultGlow = VFXClassic::readIlluminatedProjectileGlowConfigFromFile(
            "assets/data/ammo-illu-default.json",
            &defaultGlowConfig);
        selectedItem.illuminatedGlowConfig =
            loadedDefaultGlow ? defaultGlowConfig : VFXClassic::getDefaultIlluminatedProjectileGlowConfig();
        selectedItem.illuminatedGlowConfigInitialized = true;
    }
    (void)this->applySelectedProjectileGlowConfigToPanel(false);
    (void)this->applySelectedProjectileRibbonTrailConfigToPanel(false);
    this->refreshDebugPanelTrailPreview();
    this->statusMessage =
        "Boulet actif: " + this->projectileOptions[static_cast<std::size_t>(index)].displayName;
    if (this->attackerShip.areSpritesLoaded() && this->targetShip.areSpritesLoaded())
    {
        GetMaritimeCannonSalvoSystem().clear();
        this->clearLocalVfxShips();
        this->fireCurrentSalvo();
        this->salvoTimerSec = 0.0f;
    }
    return true;
}

bool EditorMapCannonSalvoScene::selectTrailAtIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->trailOptions.size()))
    {
        return false;
    }
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->statusMessage = "Selectionne d'abord un boulet pour lui affecter un trail.";
        return false;
    }

    this->selectedTrailIndex = index;
    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    item.ribbonTrailVfxFolderPath = this->trailOptions[static_cast<std::size_t>(index)].folderPath;
    item.ribbonTrailEnabled = true;
    this->refreshDebugPanelTrailPreview();
    this->statusMessage =
        "Trail actif: " + this->trailOptions[static_cast<std::size_t>(index)].displayName;
    if (this->attackerShip.areSpritesLoaded() && this->targetShip.areSpritesLoaded())
    {
        GetMaritimeCannonSalvoSystem().clear();
        this->clearLocalVfxShips();
        this->fireCurrentSalvo();
        this->salvoTimerSec = 0.0f;
    }
    return true;
}

bool EditorMapCannonSalvoScene::syncSelectedProjectileGlowConfigFromPanel(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        return false;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    const VFXClassic::IlluminatedProjectileGlowConfig config =
        VFXClassic::getIlluminatedProjectileGlowConfig();
    if (item.projectileGlowMode == ProjectileGlowMode::LIVE)
    {
        item.illuminatedGlowConfig = config;
        item.illuminatedGlowConfigInitialized = true;
    }
    this->lastObservedGlowConfig = config;
    this->lastObservedGlowConfigValid = true;
    return true;
}

bool EditorMapCannonSalvoScene::applySelectedProjectileGlowConfigToPanel(bool showStatusMessage)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        return false;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    VFXClassic::IlluminatedProjectileGlowConfig configToApply =
        VFXClassic::getDefaultIlluminatedProjectileGlowConfig();
    bool hasConfigToApply = false;

    if (item.projectileGlowMode == ProjectileGlowMode::LIVE)
    {
        if (!item.illuminatedGlowConfigInitialized)
        {
            VFXClassic::IlluminatedProjectileGlowConfig defaultGlowConfig{};
            const bool loadedDefaultGlow = VFXClassic::readIlluminatedProjectileGlowConfigFromFile(
                "assets/data/ammo-illu-default.json",
                &defaultGlowConfig);
            item.illuminatedGlowConfig =
                loadedDefaultGlow ? defaultGlowConfig : VFXClassic::getDefaultIlluminatedProjectileGlowConfig();
            item.illuminatedGlowConfigInitialized = true;
        }
        configToApply = item.illuminatedGlowConfig;
        hasConfigToApply = true;
    }
    else
    {
        const char* glowConfigPathToLoad = nullptr;
        if (item.projectileGlowMode == ProjectileGlowMode::DEFAULT_JSON)
        {
            glowConfigPathToLoad = "assets/data/ammo-illu-default.json";
        }

        if (glowConfigPathToLoad != nullptr)
        {
            VFXClassic::IlluminatedProjectileGlowConfig loadedConfig{};
            if (VFXClassic::readIlluminatedProjectileGlowConfigFromFile(
                    glowConfigPathToLoad,
                    &loadedConfig))
            {
                configToApply = loadedConfig;
                hasConfigToApply = true;
            }
            else if (showStatusMessage)
            {
                this->statusMessage =
                    "JSON glow introuvable: " +
                    shortenMiddle(normalizePathSlashes(glowConfigPathToLoad), 56U);
            }
        }
    }

    if (hasConfigToApply)
    {
        VFXClassic::setIlluminatedProjectileGlowConfig(configToApply);
        if (item.projectileGlowMode == ProjectileGlowMode::LIVE)
        {
            item.illuminatedGlowConfig = configToApply;
            item.illuminatedGlowConfigInitialized = true;
        }
    }
    this->lastObservedGlowConfig = configToApply;
    this->lastObservedGlowConfigValid = true;

    if (showStatusMessage)
    {
        if (item.projectileGlowMode == ProjectileGlowMode::DEFAULT_JSON)
        {
            this->statusMessage =
                "Glow default actif: " +
                shortenMiddle("assets/data/ammo-illu-default.json", 56U);
        }
        else
        {
            this->statusMessage =
                item.illuminatedEnabled
                    ? "Glow live actif sur le boulet selectionne."
                    : "Boulet selectionne en mode non illumine.";
        }
    }
    return true;
}

bool EditorMapCannonSalvoScene::syncSelectedProjectileRibbonTrailConfigFromPanel(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        return false;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    item.ribbonTrailConfig = MaritimeCannonSalvoSystem::getProjectileRibbonTrailConfig();
    item.ribbonTrailConfigInitialized = true;
    return true;
}

bool EditorMapCannonSalvoScene::applySelectedProjectileRibbonTrailConfigToPanel(bool showStatusMessage)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        return false;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    if (!item.ribbonTrailConfigInitialized)
    {
        item.ribbonTrailConfig = MaritimeCannonSalvoSystem::getDefaultProjectileRibbonTrailConfig();
        item.ribbonTrailConfigInitialized = true;
        if (item.ribbonTrailVfxFolderPath.empty() &&
            this->selectedTrailIndex >= 0 &&
            this->selectedTrailIndex < static_cast<int>(this->trailOptions.size()))
        {
            item.ribbonTrailVfxFolderPath =
                this->trailOptions[static_cast<std::size_t>(this->selectedTrailIndex)].folderPath;
        }
    }

    MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(item.ribbonTrailConfig);

    this->selectedTrailIndex = -1;
    for (int i = 0; i < static_cast<int>(this->trailOptions.size()); ++i)
    {
        if (this->trailOptions[static_cast<std::size_t>(i)].folderPath == item.ribbonTrailVfxFolderPath)
        {
            this->selectedTrailIndex = i;
            break;
        }
    }
    if (this->selectedTrailIndex < 0 && !this->trailOptions.empty())
    {
        this->selectedTrailIndex = 0;
        if (item.ribbonTrailVfxFolderPath.empty())
        {
            item.ribbonTrailVfxFolderPath =
                this->trailOptions[static_cast<std::size_t>(this->selectedTrailIndex)].folderPath;
        }
    }

    if (showStatusMessage)
    {
        this->statusMessage = item.ribbonTrailEnabled
            ? "Ribbon trail actif sur le boulet selectionne."
            : "Ribbon trail desactive pour ce boulet.";
    }
    this->refreshDebugPanelTrailPreview();
    return true;
}

void EditorMapCannonSalvoScene::refreshDebugPanelTrailPreview(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->illuminatedProjectileDebugPanel.setTrailPreviewSelection(nullptr, nullptr, false, false);
        return;
    }

    const ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    this->illuminatedProjectileDebugPanel.setTrailPreviewSelection(
        item.folderPath.empty() ? nullptr : item.folderPath.c_str(),
        item.ribbonTrailVfxFolderPath.empty() ? nullptr : item.ribbonTrailVfxFolderPath.c_str(),
        item.illuminatedEnabled,
        item.ribbonTrailEnabled);
}

bool EditorMapCannonSalvoScene::exportSelectedProjectileGlowConfig(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->statusMessage = "Selectionne un boulet avant l'export JSON.";
        return false;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    if (item.illuminatedGlowConfigPath.empty())
    {
        item.illuminatedGlowConfigPath = VFXClassic::getIlluminatedProjectileGlowConfigPath();
    }

    const VFXClassic::IlluminatedProjectileGlowConfig config =
        VFXClassic::getIlluminatedProjectileGlowConfig();
    const bool exported = VFXClassic::exportIlluminatedProjectileGlowConfigToFile(
        item.illuminatedGlowConfigPath.c_str(),
        &config);
    if (!exported)
    {
        this->statusMessage = "Export glow impossible.";
        return false;
    }

    item.illuminatedGlowConfig = config;
    item.illuminatedGlowConfigInitialized = true;
    MaritimeCannonSalvoSystem::invalidateIlluminatedProjectileGlowConfigFileCache(
        item.illuminatedGlowConfigPath.c_str());
    this->statusMessage =
        "Glow exporte: " + shortenMiddle(item.illuminatedGlowConfigPath, 58U);
    return true;
}

void EditorMapCannonSalvoScene::toggleSelectedProjectileIlluminated(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->statusMessage = "Selectionne un boulet pour changer son mode illumine.";
        return;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    item.illuminatedEnabled = !item.illuminatedEnabled;
    if (item.illuminatedEnabled)
    {
        // Re-applique immediatement la config glow du mode courant (LIVE/DEFAULT/JSON)
        // pour que l'activation illuminee prenne effet sans cycle manuel de mode.
        (void)this->applySelectedProjectileGlowConfigToPanel(false);
    }
    this->refreshDebugPanelTrailPreview();
    this->statusMessage = item.illuminatedEnabled
        ? "Mode illumine active pour ce boulet."
        : "Mode illumine desactive pour ce boulet.";
    this->fireCurrentSalvo();
    this->salvoTimerSec = 0.0f;
}

void EditorMapCannonSalvoScene::toggleSelectedProjectileRibbonTrail(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->statusMessage = "Selectionne un boulet pour changer sa trainée ribbon.";
        return;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    item.ribbonTrailEnabled = !item.ribbonTrailEnabled;
    if (!item.ribbonTrailConfigInitialized)
    {
        item.ribbonTrailConfig = MaritimeCannonSalvoSystem::getDefaultProjectileRibbonTrailConfig();
        item.ribbonTrailConfigInitialized = true;
    }
    this->refreshDebugPanelTrailPreview();
    this->statusMessage = item.ribbonTrailEnabled
        ? "Ribbon trail active pour ce boulet."
        : "Ribbon trail desactive pour ce boulet.";
    this->fireCurrentSalvo();
    this->salvoTimerSec = 0.0f;
}

void EditorMapCannonSalvoScene::cycleSelectedProjectileGlowMode(void)
{
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->statusMessage = "Selectionne un boulet pour changer son mode glow.";
        return;
    }

    ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    if (item.projectileGlowMode == ProjectileGlowMode::LIVE)
    {
        // Capture la config live courante avant de changer de mode.
        item.illuminatedGlowConfig = VFXClassic::getIlluminatedProjectileGlowConfig();
        item.illuminatedGlowConfigInitialized = true;
    }
    switch (item.projectileGlowMode)
    {
        case ProjectileGlowMode::LIVE:
            item.projectileGlowMode = ProjectileGlowMode::DEFAULT_JSON;
            break;
        case ProjectileGlowMode::DEFAULT_JSON:
            item.projectileGlowMode = ProjectileGlowMode::LIVE;
            break;
        default:
            item.projectileGlowMode = ProjectileGlowMode::LIVE;
            break;
    }
    (void)this->applySelectedProjectileGlowConfigToPanel(true);
    this->fireCurrentSalvo();
    this->salvoTimerSec = 0.0f;
}

bool EditorMapCannonSalvoScene::selectStartVfxAtIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->startVfxOptions.size()))
    {
        this->selectedStartVfxIndex = -1;
        this->statusMessage = "VFX depart desactive.";
        return true;
    }

    this->selectedStartVfxIndex = (this->selectedStartVfxIndex == index) ? -1 : index;
    this->statusMessage = (this->selectedStartVfxIndex >= 0)
        ? ("VFX depart: " + this->startVfxOptions[static_cast<std::size_t>(index)].displayName)
        : std::string("VFX depart desactive.");
    return true;
}

bool EditorMapCannonSalvoScene::toggleEndVfxAtIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(this->endVfxOptions.size()))
    {
        return false;
    }

    ListItem& item = this->endVfxOptions[static_cast<std::size_t>(index)];
    item.selected = !item.selected;
    this->focusedEndVfxIndex = index;
    this->refreshEndDelayInputFromFocus();
    this->statusMessage = item.selected
        ? ("Impact ajoute: " + item.displayName)
        : ("Impact retire: " + item.displayName);
    return true;
}

void EditorMapCannonSalvoScene::refreshEndDelayInputFromFocus(void)
{
    if (this->focusedEndVfxIndex < 0 ||
        this->focusedEndVfxIndex >= static_cast<int>(this->endVfxOptions.size()))
    {
        this->endDelayInputBuffer = "0";
        return;
    }

    const ListItem& item = this->endVfxOptions[static_cast<std::size_t>(this->focusedEndVfxIndex)];
    this->endDelayInputBuffer = std::to_string(item.delayAfterImpactMs);
}

void EditorMapCannonSalvoScene::setFocusedEndVfxDelay(std::uint32_t delayMs)
{
    if (this->focusedEndVfxIndex < 0 ||
        this->focusedEndVfxIndex >= static_cast<int>(this->endVfxOptions.size()))
    {
        this->statusMessage = "Selectionne un VFX impact avant de regler son delay.";
        this->refreshEndDelayInputFromFocus();
        return;
    }

    ListItem& item = this->endVfxOptions[static_cast<std::size_t>(this->focusedEndVfxIndex)];
    item.delayAfterImpactMs = delayMs;
    item.selected = true;
    this->statusMessage =
        "Delay end VFX impact " + item.displayName + ": " + std::to_string(item.delayAfterImpactMs) + " ms";
}

bool EditorMapCannonSalvoScene::applyFocusedEndVfxDelayInput(void)
{
    if (this->focusedEndVfxIndex < 0 ||
        this->focusedEndVfxIndex >= static_cast<int>(this->endVfxOptions.size()))
    {
        this->refreshEndDelayInputFromFocus();
        return false;
    }

    if (this->endDelayInputBuffer.empty())
    {
        this->endDelayInputBuffer = "0";
    }

    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(this->endDelayInputBuffer.c_str(), &end, 10);
    if (end == this->endDelayInputBuffer.c_str())
    {
        this->refreshEndDelayInputFromFocus();
        return false;
    }

    const std::uint32_t clampedDelay =
        static_cast<std::uint32_t>((std::min)(parsed, static_cast<unsigned long long>((std::numeric_limits<std::uint32_t>::max)())));
    this->setFocusedEndVfxDelay(clampedDelay);
    this->endDelayInputBuffer = std::to_string(clampedDelay);
    return true;
}

void EditorMapCannonSalvoScene::applySelectedOceanColor(void)
{
    if (this->selectedOceanColorIndex < 0)
    {
        this->selectedOceanColorIndex = 0;
    }
    if (this->selectedOceanColorIndex >= static_cast<int>(kOceanColors.size()))
    {
        this->selectedOceanColorIndex = static_cast<int>(kOceanColors.size()) - 1;
    }

    const OceanColorEntry& entry = kOceanColors[static_cast<std::size_t>(this->selectedOceanColorIndex)];
    if (!GetOceanShader().load(entry.value))
    {
        this->statusMessage = "Echec ocean: " + std::string(entry.label);
        return;
    }

    this->statusMessage = "Ocean actif: " + std::string(entry.label);
}

void EditorMapCannonSalvoScene::requestOceanColorStep(int delta)
{
    this->pendingOceanColorDelta += delta;
    this->pendingOceanColorDelta = (std::clamp)(this->pendingOceanColorDelta, -8, 8);
}

void EditorMapCannonSalvoScene::applyPendingOceanColorStep(void)
{
    if (this->pendingOceanColorDelta == 0)
    {
        return;
    }

    const int delta = this->pendingOceanColorDelta;
    this->pendingOceanColorDelta = 0;
    this->cycleOceanColor(delta);
}

void EditorMapCannonSalvoScene::cycleOceanColor(int delta)
{
    const int colorCount = static_cast<int>(kOceanColors.size());
    int index = this->selectedOceanColorIndex + delta;
    while (index < 0)
    {
        index += colorCount;
    }
    while (index >= colorCount)
    {
        index -= colorCount;
    }
    this->selectedOceanColorIndex = index;
    this->applySelectedOceanColor();
}

void EditorMapCannonSalvoScene::adjustMapZoom(float delta)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    const float zoom = (std::clamp)(camera.getZoomFactor() + delta, kEditorZoomMin, kEditorZoomMax);
    camera.setZoomFactor(zoom);
    camera.update(map, map.rect);
    char status[96] = {};
    SDL_snprintf(status, sizeof(status), "Zoom map: %.2f", zoom);
    this->statusMessage = status;
}

void EditorMapCannonSalvoScene::centerCameraOnAttacker(void)
{
    if (!this->attackerShip.areSpritesLoaded())
    {
        this->statusMessage = "Aucun navire tireur charge.";
        return;
    }

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    const SDL_FPoint tile = this->attackerShip.getPositionTile();
    camera.centerCameraOnTile(tile.x, tile.y, map, map.rect);
    camera.update(map, map.rect);
    this->statusMessage = "Camera centree sur le navire tireur.";
}

void EditorMapCannonSalvoScene::selectSalvoBallCount(int ballCount)
{
    if (ballCount != 1 && ballCount != 5 && ballCount != 10)
    {
        return;
    }

    this->selectedSalvoBallCount = ballCount;
    GetMaritimeCannonSalvoSystem().clear();
    this->clearLocalVfxShips();
    this->salvoTimerSec = 0.0f;
    this->fireCurrentSalvo();
}

void EditorMapCannonSalvoScene::adjustShipsSpeed(float delta)
{
    const float currentSpeed = this->attackerShip.getSpeedTilesPerSecond();
    const float nextSpeed = (std::clamp)(currentSpeed + delta, 0.5f, 20.0f);
    this->attackerShip.setSpeedTilesPerSecond(nextSpeed);
    this->targetShip.setSpeedTilesPerSecond(nextSpeed);
    this->statusMessage = "Vitesse navires: " + std::to_string(nextSpeed) + " tuiles/s";
}

void EditorMapCannonSalvoScene::positionShipsForPreview(bool keepExistingPositions)
{
    if (keepExistingPositions && this->attackerShip.areSpritesLoaded() && this->targetShip.areSpritesLoaded())
    {
        return;
    }

    Map& map = GetCurrentMap();
    const SDL_Point center = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    SDL_Point attacker = map.clampTile(center.x - 7, center.y);
    SDL_Point target = map.clampTile(center.x + 7, center.y);
    this->attackerShip.setPositionTileInt(attacker.x, attacker.y);
    this->targetShip.setPositionTileInt(target.x, target.y);
}

void EditorMapCannonSalvoScene::fireCurrentSalvo(void)
{
    if (!this->attackerShip.areSpritesLoaded() || !this->targetShip.areSpritesLoaded())
    {
        this->statusMessage = "Aucun navire charge pour tirer.";
        return;
    }
    if (this->selectedProjectileIndex < 0 ||
        this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
    {
        this->statusMessage = "Aucune spritesheet boulet selectionnee.";
        return;
    }

    MaritimeCannonSalvoSystem::SalvoEntry entry{};
    entry.entryId = 1U;
    entry.debugName = "editor_cannon_salvo";
    const ListItem& projectileItem =
        this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
    entry.projectileVfxClassicFolder = projectileItem.folderPath.c_str();
    entry.illuminatedProjectile.enabled = projectileItem.illuminatedEnabled;
    entry.illuminatedProjectile.glowConfigJsonPath = nullptr;
    entry.ribbonTrail.enabled = projectileItem.ribbonTrailEnabled;
    entry.ribbonTrail.trailConfigJsonPath = nullptr;
    entry.ribbonTrail.vfxClassicFolderPath =
        projectileItem.ribbonTrailVfxFolderPath.empty()
            ? nullptr
            : projectileItem.ribbonTrailVfxFolderPath.c_str();
    if (this->selectedStartVfxIndex >= 0 &&
        this->selectedStartVfxIndex < static_cast<int>(this->startVfxOptions.size()))
    {
        entry.startActionVfxShipFolderPathForAttacker =
            this->startVfxOptions[static_cast<std::size_t>(this->selectedStartVfxIndex)].folderPath.c_str();
    }

    for (const ListItem& item : this->endVfxOptions)
    {
        if (!item.selected || item.folderPath.empty())
        {
            continue;
        }

        MaritimeCannonSalvoSystem::SalvoEntry::EndActionVfxShipFolderPathForTarget endAction{};
        endAction.vfxShipFolder = item.folderPath.c_str();
        endAction.delayAfterImpactMs = item.delayAfterImpactMs;
        entry.endActionVfxShipFoldersPathForTarget.push_back(endAction);
    }

    GetMaritimeCannonSalvoSystem().fireSalvo(
        this->attackerShip,
        this->targetShip,
        entry,
        this->selectedSalvoBallCount,
        &this->localVfxShips);
    this->statusMessage =
        "Salve tiree: " + std::to_string(this->selectedSalvoBallCount) + " boulet(s).";
}

void EditorMapCannonSalvoScene::updateLocalVfxShipSlotsForDraw(void)
{
    for (GameplayVfxShipSlot& slot : this->localVfxShips)
    {
        Ship* anchorShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.attackerShip
                : slot.targetShip;
        if (anchorShip == nullptr)
        {
            continue;
        }

        const SDL_FPoint* targetTile = nullptr;
        SDL_FPoint relativeTargetTile{};
        Ship* relativeShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.targetShip
                : slot.attackerShip;
        if (relativeShip != nullptr)
        {
            relativeTargetTile = relativeShip->getPositionTile();
            targetTile = &relativeTargetTile;
        }

        slot.vfx.update(0.0, *anchorShip, targetTile);
    }
}

void EditorMapCannonSalvoScene::drawLocalVfxShipSlots(bool drawBehindShip) const
{
    const Map& map = GetCurrentMap();
    for (const GameplayVfxShipSlot& slot : this->localVfxShips)
    {
        Ship* anchorShip =
            (slot.anchorRole == GameplayVfxShipAnchorRole::ATTACKER)
                ? slot.attackerShip
                : slot.targetShip;
        if (anchorShip != nullptr)
        {
            slot.vfx.draw(map, *anchorShip, drawBehindShip);
        }
    }
}

void EditorMapCannonSalvoScene::updateToolbarLayout(void)
{
    const Map& map = GetCurrentMap();
    const SDL_FRect screenRect = GetGameScreen().rect;
    const float margin = 10.0f;
    const float gap = 6.0f;
    const float toolbarLeft = screenRect.x + margin;
    const float toolbarRight = screenRect.x + screenRect.w - margin;
    const float toolbarWidth = (std::max)(120.0f, toolbarRight - toolbarLeft);
    const float bottomBandTop = map.rect.y + map.rect.h + 6.0f;
    const float rowGap = 4.0f;
    const float totalToolbarHeight = (kTopButtonHeight * 6.0f) + (rowGap * 5.0f);
    const float row1Y = (std::min)(
        (std::max)(bottomBandTop, screenRect.y + screenRect.h - (totalToolbarHeight + margin)),
        screenRect.y + screenRect.h - (totalToolbarHeight + 2.0f));
    const float row2Y = row1Y + kTopButtonHeight + rowGap;
    const float row3Y = row2Y + kTopButtonHeight + rowGap;
    const float row4Y = row3Y + kTopButtonHeight + rowGap;
    const float row5Y = row4Y + kTopButtonHeight + rowGap;
    const float row6Y = row5Y + kTopButtonHeight + rowGap;

    auto scaledWidths = [&](std::initializer_list<float> baseWidths) {
        std::vector<float> widths(baseWidths);
        const float totalBase =
            std::accumulate(widths.begin(), widths.end(), 0.0f) +
            (gap * static_cast<float>((std::max)(static_cast<int>(widths.size()) - 1, 0)));
        const float scale = (totalBase > toolbarWidth)
            ? (std::clamp)(toolbarWidth / totalBase, 0.58f, 1.0f)
            : 1.0f;
        for (float& width : widths)
        {
            width = (std::max)(54.0f, std::floor(width * scale));
        }
        return widths;
    };

    auto placeRow = [&](const std::vector<std::pair<SDL_FRect*, float>>& entries, float y) {
        float totalWidth = gap * static_cast<float>((std::max)(static_cast<int>(entries.size()) - 1, 0));
        for (const auto& entry : entries)
        {
            totalWidth += entry.second;
        }
        float x = toolbarLeft + (std::max)(0.0f, (toolbarWidth - totalWidth) * 0.5f);
        for (const auto& entry : entries)
        {
            *entry.first = SDL_FRect{x, y, entry.second, kTopButtonHeight};
            x += entry.second + gap;
        }
    };

    const std::vector<float> row1Widths = scaledWidths({178.0f, 118.0f, 94.0f, 94.0f, 150.0f, 76.0f, 76.0f});
    placeRow(
        {
            {&this->buttonDebugPanelRect, row1Widths[0]},
            {&this->buttonListsVisibilityRect, row1Widths[1]},
            {&this->buttonOceanPrevRect, row1Widths[2]},
            {&this->buttonOceanNextRect, row1Widths[3]},
            {&this->buttonCenterAttackerRect, row1Widths[4]},
            {&this->buttonZoomOutRect, row1Widths[5]},
            {&this->buttonZoomInRect, row1Widths[6]},
        },
        row1Y);

    const std::vector<float> row2Widths = scaledWidths({198.0f, 208.0f, 218.0f});
    placeRow(
        {
            {&this->buttonSalvoOneRect, row2Widths[0]},
            {&this->buttonSalvoFiveRect, row2Widths[1]},
            {&this->buttonSalvoTenRect, row2Widths[2]},
        },
        row2Y);

    const std::vector<float> row3Widths = scaledWidths({194.0f, 194.0f, 194.0f, 194.0f});
    placeRow(
        {
            {&this->buttonShipSpeedDownRect, row3Widths[0]},
            {&this->buttonShipSpeedUpRect, row3Widths[1]},
            {&this->buttonCadenceDownRect, row3Widths[2]},
            {&this->buttonCadenceUpRect, row3Widths[3]},
        },
        row3Y);

    const std::vector<float> row4Widths = scaledWidths({252.0f, 252.0f, 252.0f});
    placeRow(
        {
            {&this->buttonProjectileIlluminatedRect, row4Widths[0]},
            {&this->buttonProjectileTrailRect, row4Widths[1]},
            {&this->buttonProjectileGlowModeRect, row4Widths[2]},
        },
        row4Y);

    this->buttonProjectileGlowImportRect = SDL_FRect{};
    this->buttonProjectileGlowExportRect = SDL_FRect{};

    const std::vector<float> row6Widths = scaledWidths({252.0f});
    placeRow(
        {
            {&this->endDelayInputRect, row6Widths[0]},
        },
        row6Y);

    const float topUiLimit = map.rect.y + 10.0f;
    const float listTopLimit = topUiLimit;
    const float listBottomLimit = map.rect.y + map.rect.h - 14.0f;
    const float listAvailableH = (std::max)(160.0f, listBottomLimit - listTopLimit);
    const float panelH = (std::max)(72.0f, (listAvailableH - (kRightPanelGap * 4.0f)) / 5.0f);
    const float rightX = (std::min)(
        map.rect.x + map.rect.w - kRightPanelWidth - 14.0f,
        screenRect.x + screenRect.w - kRightPanelWidth - margin);
    float panelY = listBottomLimit - ((panelH * 5.0f) + (kRightPanelGap * 4.0f));
    panelY = (std::max)(panelY, listTopLimit);

    this->shipListRect = SDL_FRect{rightX, panelY, kRightPanelWidth, panelH};
    panelY += panelH + kRightPanelGap;
    this->projectileListRect = SDL_FRect{rightX, panelY, kRightPanelWidth, panelH};
    panelY += panelH + kRightPanelGap;
    this->trailListRect = SDL_FRect{rightX, panelY, kRightPanelWidth, panelH};
    panelY += panelH + kRightPanelGap;
    this->startVfxListRect = SDL_FRect{rightX, panelY, kRightPanelWidth, panelH};
    panelY += panelH + kRightPanelGap;
    this->endVfxListRect = SDL_FRect{rightX, panelY, kRightPanelWidth, panelH};

    auto placeImportButton = [](const SDL_FRect& panel, float width) {
        return SDL_FRect{
            panel.x + kListPadding,
            panel.y + panel.h - kListPadding - kListImportButtonHeight,
            width,
            kListImportButtonHeight};
    };
    this->projectileImportButtonRect = placeImportButton(this->projectileListRect, this->projectileListRect.w - (kListPadding * 2.0f));
    this->trailImportButtonRect = placeImportButton(this->trailListRect, this->trailListRect.w - (kListPadding * 2.0f));
    this->startVfxImportButtonRect = placeImportButton(this->startVfxListRect, this->startVfxListRect.w - (kListPadding * 2.0f));
    this->endVfxImportButtonRect = placeImportButton(this->endVfxListRect, this->endVfxListRect.w - (kListPadding * 2.0f));

    const float miniMapSize = computeEditorMiniMapSize(map.rect);
    this->miniMapRect.w = miniMapSize;
    this->miniMapRect.h = miniMapSize;
    this->miniMapRect.x = map.rect.x + map.rect.w - miniMapSize - 40.0f;
    this->miniMapRect.y = map.rect.y + 40.0f;

    this->clampListScrollOffset(&this->shipListScrollOffset, this->shipListRect, false, static_cast<int>(this->shipOptions.size()));
    this->clampListScrollOffset(&this->projectileListScrollOffset, this->projectileListRect, true, static_cast<int>(this->projectileOptions.size()));
    this->clampListScrollOffset(&this->trailListScrollOffset, this->trailListRect, true, static_cast<int>(this->trailOptions.size()));
    this->clampListScrollOffset(&this->startVfxListScrollOffset, this->startVfxListRect, true, static_cast<int>(this->startVfxOptions.size()));
    this->clampListScrollOffset(&this->endVfxListScrollOffset, this->endVfxListRect, true, static_cast<int>(this->endVfxOptions.size()));
}

void EditorMapCannonSalvoScene::drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(active ? kButtonFillActiveColor : kButtonFillColor);
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setColor(active ? kButtonBorderActiveColor : kButtonBorderColor);
    rc2d_graphics_rectangle("line", &rect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    if (this->overlayFont.sdl_font == nullptr || label == nullptr)
    {
        return;
    }

    const std::string sourceLabel = label;
    std::string fittedLabel = sourceLabel;
    RC2D_Text text = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), fittedLabel.c_str());
    text.color = kHudTextColor;
    rc2d_graphics_setTextColor(&text);
    int textW = 0;
    int textH = 0;
    rc2d_graphics_getTextSize(&text, &textW, &textH);
    const int maxTextW = (std::max)(8, static_cast<int>(std::floor(rect.w - 10.0f)));
    std::size_t fitChars = sourceLabel.size();
    while (textW > maxTextW && fitChars > 1U)
    {
        rc2d_graphics_destroyText(&text);
        --fitChars;
        fittedLabel = (fitChars > 3U)
            ? (sourceLabel.substr(0U, fitChars - 3U) + "...")
            : sourceLabel.substr(0U, fitChars);
        text = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), fittedLabel.c_str());
        text.color = kHudTextColor;
        rc2d_graphics_setTextColor(&text);
        rc2d_graphics_getTextSize(&text, &textW, &textH);
    }
    rc2d_graphics_drawText(
        &text,
        rect.x + (std::max)(5.0f, ((rect.w - static_cast<float>(textW)) * 0.5f)),
        rect.y + ((rect.h - static_cast<float>(textH)) * 0.5f));
    rc2d_graphics_destroyText(&text);
}

void EditorMapCannonSalvoScene::drawTextLine(const char* text, float x, float y, RC2D_Color color) const
{
    if (this->overlayFont.sdl_font == nullptr || text == nullptr)
    {
        return;
    }

    RC2D_Text rendered = rc2d_graphics_createText(const_cast<RC2D_Font*>(&this->overlayFont), text);
    rendered.color = color;
    rc2d_graphics_setTextColor(&rendered);
    rc2d_graphics_drawText(&rendered, x, y);
    rc2d_graphics_destroyText(&rendered);
}

void EditorMapCannonSalvoScene::drawDelayInput(void) const
{
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(this->endDelayInputFocused ? kButtonFillActiveColor : kButtonFillColor);
    rc2d_graphics_rectangle("fill", &this->endDelayInputRect);
    rc2d_graphics_setColor(this->endDelayInputFocused ? kButtonBorderActiveColor : kButtonBorderColor);
    rc2d_graphics_rectangle("line", &this->endDelayInputRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    const std::string label =
        "Delay end VFX impact: " +
        (this->endDelayInputBuffer.empty() ? std::string("0") : this->endDelayInputBuffer) +
        " ms";
    this->drawTextLine(label.c_str(), this->endDelayInputRect.x + 8.0f, this->endDelayInputRect.y + 5.0f, kHudTextColor);
}

void EditorMapCannonSalvoScene::drawListPanel(
    const SDL_FRect& rect,
    const char* title,
    const std::vector<ListItem>& items,
    int selectedIndex,
    int scrollOffset,
    bool scrollDragActive,
    bool multiSelect,
    bool showDelay,
    const SDL_FRect* importButtonRect,
    const char* importLabel) const
{
    const bool hasImportButton = (importButtonRect != nullptr);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(kPanelFillColor);
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setColor(kPanelBorderColor);
    rc2d_graphics_rectangle("line", &rect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    this->drawTextLine(title, rect.x + kListPadding, rect.y + 2.0f, kHudTextColor);

    const int visibleRows = this->visibleRowsForListPanel(rect, hasImportButton);
    const int maxOffset = this->maxScrollOffsetForList(rect, hasImportButton, static_cast<int>(items.size()));
    const int startIndex = (std::clamp)(scrollOffset, 0, maxOffset);
    const float rowsTop = rect.y + kListPadding + kListHeaderHeight;
    const float rowsLeft = rect.x + kListPadding;
    const float rowsWidth = rect.w - (kListPadding * 2.0f) - kListScrollBarWidth - 4.0f;

    for (int row = 0; row < visibleRows; ++row)
    {
        const int itemIndex = startIndex + row;
        if (itemIndex >= static_cast<int>(items.size()))
        {
            break;
        }

        const ListItem& item = items[static_cast<std::size_t>(itemIndex)];
        const SDL_FRect rowRect{
            rowsLeft,
            rowsTop + (static_cast<float>(row) * (kListRowHeight + kListRowGap)),
            rowsWidth,
            kListRowHeight};
        const bool selected = multiSelect ? item.selected : (itemIndex == selectedIndex);
        const bool focused = multiSelect && itemIndex == this->focusedEndVfxIndex;

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(selected ? (multiSelect ? kRowMultiSelectedFillColor : kRowSelectedFillColor) : kRowFillColor);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(focused ? kRowFocusedBorderColor : kButtonBorderColor);
        rc2d_graphics_rectangle("line", &rowRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        std::string label = item.displayName;
        if (!multiSelect && !showDelay && title != nullptr && startsWith(title, "Boulets "))
        {
            if (!item.illuminatedEnabled)
            {
                label = "[OFF] " + label;
            }
            else if (item.projectileGlowMode == ProjectileGlowMode::DEFAULT_JSON)
            {
                label = "[DEFAULT] " + label;
            }
            else
            {
                label = "[LIVE] " + label;
            }
        }
        if (showDelay && item.delayAfterImpactMs > 0U)
        {
            label += " +" + std::to_string(item.delayAfterImpactMs) + "ms";
        }
        label = shortenMiddle(label, 38U);
        this->drawTextLine(label.c_str(), rowRect.x + 5.0f, rowRect.y + 2.0f, selected ? kHudTextColor : kHudMutedTextColor);
    }

    if (items.empty())
    {
        this->drawTextLine("Aucun asset trouve", rowsLeft, rowsTop + 2.0f, kHudMutedTextColor);
    }

    const float rowsHeight =
        (static_cast<float>(visibleRows) * kListRowHeight) +
        (static_cast<float>((std::max)(visibleRows - 1, 0)) * kListRowGap);
    const SDL_FRect scrollTrackRect{
        rowsLeft + rowsWidth + 4.0f,
        rowsTop,
        kListScrollBarWidth,
        rowsHeight};
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{16, 22, 30, 190});
    rc2d_graphics_rectangle("fill", &scrollTrackRect);
    rc2d_graphics_setColor(RC2D_Color{105, 122, 142, 210});
    rc2d_graphics_rectangle("line", &scrollTrackRect);
    if (static_cast<int>(items.size()) > visibleRows)
    {
        const int safeMaxOffset = (std::max)(maxOffset, 1);
        const float thumbHeight = (std::max)(
            14.0f,
            (scrollTrackRect.h * static_cast<float>(visibleRows)) /
                static_cast<float>((std::max)(static_cast<int>(items.size()), 1)));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>(safeMaxOffset);
        SDL_FRect thumbRect{
            scrollTrackRect.x + 1.0f,
            scrollTrackRect.y + (ratio * thumbTravel),
            scrollTrackRect.w - 2.0f,
            thumbHeight};
        rc2d_graphics_setColor(scrollDragActive ? kButtonBorderActiveColor : kButtonBorderColor);
        rc2d_graphics_rectangle("fill", &thumbRect);
    }
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    if (hasImportButton)
    {
        this->drawToolbarButton(*importButtonRect, importLabel, false);
    }
}

int EditorMapCannonSalvoScene::visibleRowsForListPanel(const SDL_FRect& rect, bool hasImportButton) const
{
    const float bottomReserve = hasImportButton ? (kListImportButtonHeight + (kListPadding * 2.0f)) : kListPadding;
    const float rowsHeight = rect.h - kListHeaderHeight - kListPadding - bottomReserve;
    return (std::max)(1, static_cast<int>(std::floor((rowsHeight + kListRowGap) / (kListRowHeight + kListRowGap))));
}

int EditorMapCannonSalvoScene::maxScrollOffsetForList(const SDL_FRect& rect, bool hasImportButton, int itemCount) const
{
    return (std::max)(0, itemCount - this->visibleRowsForListPanel(rect, hasImportButton));
}

void EditorMapCannonSalvoScene::clampListScrollOffset(int* scrollOffset, const SDL_FRect& rect, bool hasImportButton, int itemCount) const
{
    if (scrollOffset == nullptr)
    {
        return;
    }
    *scrollOffset = (std::clamp)(*scrollOffset, 0, this->maxScrollOffsetForList(rect, hasImportButton, itemCount));
}

bool EditorMapCannonSalvoScene::handleListPanelClick(
    float x,
    float y,
    const SDL_FRect& rect,
    bool hasImportButton,
    int itemCount,
    int* scrollOffset,
    bool* dragActive,
    float* dragGrabOffsetY,
    int* outClickedIndex) const
{
    if (outClickedIndex != nullptr)
    {
        *outClickedIndex = -1;
    }
    if (!this->pointInRect(x, y, rect))
    {
        return false;
    }

    const int visibleRows = this->visibleRowsForListPanel(rect, hasImportButton);
    const int maxOffset = this->maxScrollOffsetForList(rect, hasImportButton, itemCount);
    const int startIndex = (std::clamp)((scrollOffset != nullptr) ? *scrollOffset : 0, 0, maxOffset);
    const float rowsTop = rect.y + kListPadding + kListHeaderHeight;
    const float rowsLeft = rect.x + kListPadding;
    const float rowsWidth = rect.w - (kListPadding * 2.0f) - kListScrollBarWidth - 4.0f;
    const float rowsHeight =
        (static_cast<float>(visibleRows) * kListRowHeight) +
        (static_cast<float>((std::max)(visibleRows - 1, 0)) * kListRowGap);
    const SDL_FRect scrollTrackRect{
        rowsLeft + rowsWidth + 4.0f,
        rowsTop,
        kListScrollBarWidth,
        rowsHeight};

    if (this->pointInRect(x, y, scrollTrackRect))
    {
        if (maxOffset <= 0 || scrollOffset == nullptr)
        {
            if (dragActive != nullptr)
            {
                *dragActive = false;
            }
            return true;
        }

        const float thumbHeight = (std::max)(
            14.0f,
            (scrollTrackRect.h * static_cast<float>(visibleRows)) /
                static_cast<float>((std::max)(itemCount, 1)));
        const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
        const float ratio = static_cast<float>(startIndex) / static_cast<float>((std::max)(maxOffset, 1));
        const SDL_FRect thumbRect{
            scrollTrackRect.x + 1.0f,
            scrollTrackRect.y + (ratio * thumbTravel),
            scrollTrackRect.w - 2.0f,
            thumbHeight};

        if (this->pointInRect(x, y, thumbRect))
        {
            if (dragActive != nullptr)
            {
                *dragActive = true;
            }
            if (dragGrabOffsetY != nullptr)
            {
                *dragGrabOffsetY = y - thumbRect.y;
            }
        }
        else
        {
            const float targetThumbY = (std::clamp)(
                y - (thumbRect.h * 0.5f),
                scrollTrackRect.y,
                scrollTrackRect.y + thumbTravel);
            const float clickRatio =
                (thumbTravel > 0.0f) ? ((targetThumbY - scrollTrackRect.y) / thumbTravel) : 0.0f;
            *scrollOffset = static_cast<int>(std::round(clickRatio * static_cast<float>(maxOffset)));
            this->clampListScrollOffset(scrollOffset, rect, hasImportButton, itemCount);
            if (dragActive != nullptr)
            {
                *dragActive = true;
            }
            if (dragGrabOffsetY != nullptr)
            {
                *dragGrabOffsetY = thumbRect.h * 0.5f;
            }
        }
        return true;
    }

    for (int row = 0; row < visibleRows; ++row)
    {
        const int itemIndex = startIndex + row;
        if (itemIndex >= itemCount)
        {
            break;
        }

        const SDL_FRect rowRect{
            rowsLeft,
            rowsTop + (static_cast<float>(row) * (kListRowHeight + kListRowGap)),
            rowsWidth,
            kListRowHeight};
        if (this->pointInRect(x, y, rowRect))
        {
            if (outClickedIndex != nullptr)
            {
                *outClickedIndex = itemIndex;
            }
            return true;
        }
    }

    return true;
}

void EditorMapCannonSalvoScene::handleListPanelScrollDragFromMouse(
    const SDL_FRect& rect,
    bool hasImportButton,
    int itemCount,
    int* scrollOffset,
    bool* dragActive,
    float* dragGrabOffsetY)
{
    if (dragActive == nullptr || !(*dragActive))
    {
        return;
    }
    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        *dragActive = false;
        if (dragGrabOffsetY != nullptr)
        {
            *dragGrabOffsetY = 0.0f;
        }
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }
    (void)mouseX;

    const int visibleRows = this->visibleRowsForListPanel(rect, hasImportButton);
    const int maxOffset = this->maxScrollOffsetForList(rect, hasImportButton, itemCount);
    if (maxOffset <= 0 || scrollOffset == nullptr)
    {
        *dragActive = false;
        return;
    }

    const float rowsTop = rect.y + kListPadding + kListHeaderHeight;
    const float rowsLeft = rect.x + kListPadding;
    const float rowsWidth = rect.w - (kListPadding * 2.0f) - kListScrollBarWidth - 4.0f;
    const float rowsHeight =
        (static_cast<float>(visibleRows) * kListRowHeight) +
        (static_cast<float>((std::max)(visibleRows - 1, 0)) * kListRowGap);
    const SDL_FRect scrollTrackRect{
        rowsLeft + rowsWidth + 4.0f,
        rowsTop,
        kListScrollBarWidth,
        rowsHeight};
    const float thumbHeight = (std::max)(
        14.0f,
        (scrollTrackRect.h * static_cast<float>(visibleRows)) /
            static_cast<float>((std::max)(itemCount, 1)));
    const float thumbTravel = (std::max)(scrollTrackRect.h - thumbHeight, 0.0f);
    const float targetThumbY = (std::clamp)(
        mouseY - ((dragGrabOffsetY != nullptr) ? *dragGrabOffsetY : 0.0f),
        scrollTrackRect.y,
        scrollTrackRect.y + thumbTravel);
    const float ratio = (thumbTravel > 0.0f)
        ? ((targetThumbY - scrollTrackRect.y) / thumbTravel)
        : 0.0f;
    *scrollOffset = static_cast<int>(std::round(ratio * static_cast<float>(maxOffset)));
    this->clampListScrollOffset(scrollOffset, rect, hasImportButton, itemCount);
}

bool EditorMapCannonSalvoScene::handleToolbarClick(float x, float y)
{
    if (this->pointInRect(x, y, this->buttonDebugPanelRect))
    {
        this->illuminatedProjectileDebugPanel.toggleVisibility();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonListsVisibilityRect))
    {
        this->showLists = !this->showLists;
        this->statusMessage = this->showLists ? "Listes affichees." : "Listes masquees.";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonOceanPrevRect))
    {
        this->requestOceanColorStep(-1);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonOceanNextRect))
    {
        this->requestOceanColorStep(1);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonCenterAttackerRect))
    {
        this->centerCameraOnAttacker();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonZoomOutRect))
    {
        this->adjustMapZoom(-0.05f);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonZoomInRect))
    {
        this->adjustMapZoom(0.05f);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonSalvoOneRect))
    {
        this->selectSalvoBallCount(1);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonSalvoFiveRect))
    {
        this->selectSalvoBallCount(5);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonSalvoTenRect))
    {
        this->selectSalvoBallCount(10);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonShipSpeedDownRect))
    {
        this->adjustShipsSpeed(-0.5f);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonShipSpeedUpRect))
    {
        this->adjustShipsSpeed(0.5f);
        return true;
    }
    if (this->pointInRect(x, y, this->buttonCadenceDownRect))
    {
        this->salvoIntervalSec = (std::clamp)(this->salvoIntervalSec + 0.25f, 0.25f, 20.0f);
        this->statusMessage = "Cadence attack: " + std::to_string(this->salvoIntervalSec) + " s";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonCadenceUpRect))
    {
        this->salvoIntervalSec = (std::clamp)(this->salvoIntervalSec - 0.25f, 0.25f, 20.0f);
        this->statusMessage = "Cadence attack: " + std::to_string(this->salvoIntervalSec) + " s";
        return true;
    }
    if (this->pointInRect(x, y, this->buttonProjectileIlluminatedRect))
    {
        this->toggleSelectedProjectileIlluminated();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonProjectileTrailRect))
    {
        this->toggleSelectedProjectileRibbonTrail();
        return true;
    }
    if (this->pointInRect(x, y, this->buttonProjectileGlowModeRect))
    {
        this->cycleSelectedProjectileGlowMode();
        return true;
    }
    if (this->pointInRect(x, y, this->endDelayInputRect))
    {
        this->endDelayInputFocused = true;
        this->refreshEndDelayInputFromFocus();
        this->syncEditorTextInputState();
        if (this->focusedEndVfxIndex < 0)
        {
            this->statusMessage = "Selectionne un End VFX avant de saisir son delay.";
        }
        return true;
    }

    return false;
}

bool EditorMapCannonSalvoScene::handleAssetListClick(float x, float y)
{
    if (!this->showLists)
    {
        return false;
    }

    if (this->pointInRect(x, y, this->projectileImportButtonRect))
    {
        this->openImportFolderDialog(ImportTarget::PROJECTILE);
        return true;
    }
    if (this->pointInRect(x, y, this->trailImportButtonRect))
    {
        this->openImportFolderDialog(ImportTarget::TRAIL);
        return true;
    }
    if (this->pointInRect(x, y, this->startVfxImportButtonRect))
    {
        this->openImportFolderDialog(ImportTarget::START_VFX);
        return true;
    }
    if (this->pointInRect(x, y, this->endVfxImportButtonRect))
    {
        this->openImportFolderDialog(ImportTarget::END_VFX);
        return true;
    }

    int clicked = -1;
    if (this->handleListPanelClick(
            x,
            y,
            this->shipListRect,
            false,
            static_cast<int>(this->shipOptions.size()),
            &this->shipListScrollOffset,
            &this->shipListScrollDragActive,
            &this->shipListScrollDragGrabOffsetY,
            &clicked))
    {
        if (clicked >= 0)
        {
            (void)this->selectShipAtIndex(clicked);
        }
        return true;
    }
    if (this->handleListPanelClick(
            x,
            y,
            this->projectileListRect,
            true,
            static_cast<int>(this->projectileOptions.size()),
            &this->projectileListScrollOffset,
            &this->projectileListScrollDragActive,
            &this->projectileListScrollDragGrabOffsetY,
            &clicked))
    {
        if (clicked >= 0)
        {
            (void)this->selectProjectileAtIndex(clicked);
        }
        return true;
    }
    if (this->handleListPanelClick(
            x,
            y,
            this->trailListRect,
            true,
            static_cast<int>(this->trailOptions.size()),
            &this->trailListScrollOffset,
            &this->trailListScrollDragActive,
            &this->trailListScrollDragGrabOffsetY,
            &clicked))
    {
        if (clicked >= 0)
        {
            (void)this->selectTrailAtIndex(clicked);
        }
        return true;
    }
    if (this->handleListPanelClick(
            x,
            y,
            this->startVfxListRect,
            true,
            static_cast<int>(this->startVfxOptions.size()),
            &this->startVfxListScrollOffset,
            &this->startVfxListScrollDragActive,
            &this->startVfxListScrollDragGrabOffsetY,
            &clicked))
    {
        if (clicked >= 0)
        {
            (void)this->selectStartVfxAtIndex(clicked);
        }
        return true;
    }
    if (this->handleListPanelClick(
            x,
            y,
            this->endVfxListRect,
            true,
            static_cast<int>(this->endVfxOptions.size()),
            &this->endVfxListScrollOffset,
            &this->endVfxListScrollDragActive,
            &this->endVfxListScrollDragGrabOffsetY,
            &clicked))
    {
        if (clicked >= 0)
        {
            (void)this->toggleEndVfxAtIndex(clicked);
        }
        return true;
    }

    return false;
}

bool EditorMapCannonSalvoScene::handleMapClick(float x, float y, RC2D_MouseButton button)
{
    Map& map = GetCurrentMap();
    if (!this->pointInRect(x, y, map.rect))
    {
        return false;
    }
    if (button != RC2D_MOUSE_BUTTON_LEFT && button != RC2D_MOUSE_BUTTON_RIGHT)
    {
        return false;
    }

    const SDL_Point tile = map.screenToTileNearest(x, y);
    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->clickMarker.show(tile.x, tile.y);
    }

    if (!map.isInside(tile.x, tile.y) || map.isTileBlocked(tile.x, tile.y))
    {
        return true;
    }

    if (button == RC2D_MOUSE_BUTTON_LEFT)
    {
        this->attackerShip.moveToTile(map, tile.x, tile.y);
        this->statusMessage = "Attaquant deplace.";
    }
    else
    {
        this->targetShip.moveToTile(map, tile.x, tile.y);
        this->statusMessage = "Cible deplacee.";
    }
    return true;
}

bool EditorMapCannonSalvoScene::handleListMouseWheel(float mouseX, float mouseY, int delta)
{
    if (!this->showLists)
    {
        return false;
    }

    auto applyWheel = [&](SDL_FRect rect, bool hasImportButton, int itemCount, int* scrollOffset) -> bool {
        if (!this->pointInRect(mouseX, mouseY, rect))
        {
            return false;
        }
        *scrollOffset -= delta;
        this->clampListScrollOffset(scrollOffset, rect, hasImportButton, itemCount);
        return true;
    };

    if (applyWheel(this->shipListRect, false, static_cast<int>(this->shipOptions.size()), &this->shipListScrollOffset))
    {
        return true;
    }
    if (applyWheel(this->projectileListRect, true, static_cast<int>(this->projectileOptions.size()), &this->projectileListScrollOffset))
    {
        return true;
    }
    if (applyWheel(this->trailListRect, true, static_cast<int>(this->trailOptions.size()), &this->trailListScrollOffset))
    {
        return true;
    }
    if (applyWheel(this->startVfxListRect, true, static_cast<int>(this->startVfxOptions.size()), &this->startVfxListScrollOffset))
    {
        return true;
    }
    if (applyWheel(this->endVfxListRect, true, static_cast<int>(this->endVfxOptions.size()), &this->endVfxListScrollOffset))
    {
        return true;
    }
    return false;
}

bool EditorMapCannonSalvoScene::handleEndDelayInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    bool isrepeat)
{
    (void)key;
    (void)keycode;

    if (!this->endDelayInputFocused)
    {
        return false;
    }

    if (!isrepeat && scancode == SDL_SCANCODE_ESCAPE)
    {
        (void)this->applyFocusedEndVfxDelayInput();
        this->endDelayInputFocused = false;
        this->syncEditorTextInputState();
        return true;
    }
    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        (void)this->applyFocusedEndVfxDelayInput();
        this->endDelayInputFocused = false;
        this->syncEditorTextInputState();
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_BACKSPACE)
    {
        if (!this->endDelayInputBuffer.empty())
        {
            this->endDelayInputBuffer.pop_back();
            (void)this->applyFocusedEndVfxDelayInput();
        }
        return true;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_DELETE)
    {
        this->endDelayInputBuffer.clear();
        (void)this->applyFocusedEndVfxDelayInput();
        return true;
    }

    return true;
}

void EditorMapCannonSalvoScene::syncEditorTextInputState(void)
{
    const bool shouldEnableTextInput = this->endDelayInputFocused;
    if (shouldEnableTextInput == this->editorTextInputEnabled)
    {
        return;
    }

    rc2d_keyboard_setTextInput(shouldEnableTextInput);
    this->editorTextInputEnabled = shouldEnableTextInput;
}

bool EditorMapCannonSalvoScene::tryBuildMiniMapViewRect(SDL_FRect* outRect) const
{
    if (outRect == nullptr || this->miniMapRect.w <= 0.0f || this->miniMapRect.h <= 0.0f)
    {
        return false;
    }

    const Map& map = GetCurrentMap();
    const float sectorSpanX = static_cast<float>((std::max)(Map::NUM_SECTORS_X, 1));
    const float sectorSpanY = static_cast<float>((std::max)(Map::NUM_SECTORS_Y, 1));
    const float halfTileW = map.getTileWidth() * 0.5f;
    const float halfTileH = map.getTileHeight() * 0.5f;
    if (halfTileW <= 0.0f || halfTileH <= 0.0f)
    {
        return false;
    }

    const float viewCenterScreenX = map.rect.x + (map.rect.w * 0.5f);
    const float viewCenterScreenY = map.rect.y + (map.rect.h * 0.5f);
    const SDL_FPoint centerTile = map.screenToTile(viewCenterScreenX, viewCenterScreenY);
    const SDL_FPoint centerSector = map.tileToSectorFloat(centerTile.x, centerTile.y);

    const float halfViewU = (map.rect.w * 0.5f) / halfTileW;
    const float halfViewV = (map.rect.h * 0.5f) / halfTileH;
    const float halfSectorX = halfViewU / (2.0f * static_cast<float>(Map::SECTOR_STEP));
    const float halfSectorY = halfViewV / (2.0f * static_cast<float>(Map::SECTOR_STEP));

    const float minSectorCenterX = centerSector.x - halfSectorX;
    const float maxSectorCenterX = centerSector.x + halfSectorX;
    const float minSectorCenterY = centerSector.y - halfSectorY;
    const float maxSectorCenterY = centerSector.y + halfSectorY;

    float minSectorEdgeX = std::clamp(minSectorCenterX + 0.5f, 0.0f, sectorSpanX);
    float maxSectorEdgeX = std::clamp(maxSectorCenterX + 0.5f, 0.0f, sectorSpanX);
    float minSectorEdgeY = std::clamp(minSectorCenterY + 0.5f, 0.0f, sectorSpanY);
    float maxSectorEdgeY = std::clamp(maxSectorCenterY + 0.5f, 0.0f, sectorSpanY);

    if (maxSectorEdgeX < minSectorEdgeX)
    {
        std::swap(minSectorEdgeX, maxSectorEdgeX);
    }
    if (maxSectorEdgeY < minSectorEdgeY)
    {
        std::swap(minSectorEdgeY, maxSectorEdgeY);
    }

    SDL_FRect viewRect{};
    viewRect.x = this->miniMapRect.x + ((minSectorEdgeX / sectorSpanX) * this->miniMapRect.w);
    viewRect.y = this->miniMapRect.y + ((minSectorEdgeY / sectorSpanY) * this->miniMapRect.h);
    viewRect.w = ((maxSectorEdgeX - minSectorEdgeX) / sectorSpanX) * this->miniMapRect.w;
    viewRect.h = ((maxSectorEdgeY - minSectorEdgeY) / sectorSpanY) * this->miniMapRect.h;
    viewRect.w = (std::max)(viewRect.w, 2.0f);
    viewRect.h = (std::max)(viewRect.h, 2.0f);
    viewRect.x = std::clamp(viewRect.x, this->miniMapRect.x, this->miniMapRect.x + this->miniMapRect.w - viewRect.w);
    viewRect.y = std::clamp(viewRect.y, this->miniMapRect.y, this->miniMapRect.y + this->miniMapRect.h - viewRect.h);

    *outRect = viewRect;
    return true;
}

void EditorMapCannonSalvoScene::moveCameraFromMiniMapPoint(float miniMapX, float miniMapY, bool applyDragOffset)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    float localX = miniMapX - this->miniMapRect.x;
    float localY = miniMapY - this->miniMapRect.y;
    if (applyDragOffset)
    {
        localX -= this->miniMapDragOffsetX;
        localY -= this->miniMapDragOffsetY;
    }

    const float nx = std::clamp(localX / (std::max)(this->miniMapRect.w, 1.0f), 0.0f, 1.0f);
    const float ny = std::clamp(localY / (std::max)(this->miniMapRect.h, 1.0f), 0.0f, 1.0f);

    const float targetSectorX = miniMapNormalizedToSectorCenter(nx, Map::NUM_SECTORS_X);
    const float targetSectorY = miniMapNormalizedToSectorCenter(ny, Map::NUM_SECTORS_Y);
    const float targetTileX =
        static_cast<float>(Map::SECTOR_BASE_X) +
        ((targetSectorX + targetSectorY) * static_cast<float>(Map::SECTOR_STEP));
    const float targetTileY =
        static_cast<float>(Map::SECTOR_BASE_Y) +
        ((targetSectorY - targetSectorX) * static_cast<float>(Map::SECTOR_STEP));

    camera.centerCameraOnTile(targetTileX, targetTileY, map, map.rect);
    camera.update(map, map.rect);
}

bool EditorMapCannonSalvoScene::handleMiniMapClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->pointInRect(x, y, this->miniMapRect))
    {
        return false;
    }

    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return true;
    }

    SDL_FRect viewRect{};
    if (this->tryBuildMiniMapViewRect(&viewRect) && this->pointInRect(x, y, viewRect))
    {
        const float viewCenterX = viewRect.x + (viewRect.w * 0.5f);
        const float viewCenterY = viewRect.y + (viewRect.h * 0.5f);
        this->miniMapDragOffsetX = x - viewCenterX;
        this->miniMapDragOffsetY = y - viewCenterY;
    }
    else
    {
        this->miniMapDragOffsetX = 0.0f;
        this->miniMapDragOffsetY = 0.0f;
        this->moveCameraFromMiniMapPoint(x, y, true);
    }

    this->miniMapDragActive = true;
    return true;
}

void EditorMapCannonSalvoScene::handleMiniMapDragFromMouse(void)
{
    if (!this->miniMapDragActive)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->miniMapDragActive = false;
        this->miniMapDragOffsetX = 0.0f;
        this->miniMapDragOffsetY = 0.0f;
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    if (!this->getMouseRenderPosition(&mouseX, &mouseY))
    {
        return;
    }

    this->moveCameraFromMiniMapPoint(mouseX, mouseY, true);
}

void EditorMapCannonSalvoScene::drawMiniMap(void) const
{
    const float left = this->miniMapRect.x;
    const float top = this->miniMapRect.y;
    const float width = this->miniMapRect.w;
    const float height = this->miniMapRect.h;
    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{28, 63, 103, 235});
    rc2d_graphics_rectangle("fill", &this->miniMapRect);

    rc2d_graphics_setColor(RC2D_Color{135, 150, 168, 235});
    rc2d_graphics_rectangle("line", &this->miniMapRect);

    SDL_FRect viewRect{};
    if (this->tryBuildMiniMapViewRect(&viewRect))
    {
        rc2d_graphics_setColor(RC2D_Color{125, 198, 255, 55});
        rc2d_graphics_rectangle("fill", &viewRect);
        rc2d_graphics_setColor(RC2D_Color{170, 222, 255, 245});
        rc2d_graphics_rectangle("line", &viewRect);
    }

    const Map& map = GetCurrentMap();
    auto drawShipDot = [&](const Ship& ship, RC2D_Color color, float size) {
        if (!ship.areSpritesLoaded())
        {
            return;
        }

        const SDL_FPoint tile = ship.getPositionTile();
        const SDL_FPoint sector = map.tileToSectorFloat(tile.x, tile.y);
        const float nx = (sector.x + 0.5f) / static_cast<float>(Map::NUM_SECTORS_X);
        const float ny = (sector.y + 0.5f) / static_cast<float>(Map::NUM_SECTORS_Y);
        if (!std::isfinite(nx) || !std::isfinite(ny) || nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f)
        {
            return;
        }

        SDL_FRect dot{};
        dot.w = size;
        dot.h = size;
        dot.x = left + (nx * width) - (dot.w * 0.5f);
        dot.y = top + (ny * height) - (dot.h * 0.5f);
        rc2d_graphics_setColor(color);
        rc2d_graphics_rectangle("fill", &dot);
    };

    drawShipDot(this->attackerShip, RC2D_Color{245, 205, 76, 255}, 4.0f);
    drawShipDot(this->targetShip, RC2D_Color{245, 65, 65, 255}, 4.0f);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapCannonSalvoScene::showBlockingPopup(const std::string& message)
{
    this->blockingPopupVisible = true;
    this->blockingPopupMessage = message;
}

bool EditorMapCannonSalvoScene::handleBlockingPopupClick(float x, float y, RC2D_MouseButton button)
{
    if (!this->blockingPopupVisible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return this->blockingPopupVisible;
    }

    const SDL_FRect screenRect = GetGameScreen().rect;
    const float popupW = (std::min)(760.0f, screenRect.w - 40.0f);
    const float popupH = (std::min)(270.0f, screenRect.h - 40.0f);
    const SDL_FRect popupRect{
        screenRect.x + ((screenRect.w - popupW) * 0.5f),
        screenRect.y + ((screenRect.h - popupH) * 0.5f),
        popupW,
        popupH};
    const SDL_FRect okButtonRect{
        popupRect.x + popupRect.w - 124.0f,
        popupRect.y + popupRect.h - 52.0f,
        104.0f,
        32.0f};
    if (this->pointInRect(x, y, okButtonRect))
    {
        this->blockingPopupVisible = false;
        this->blockingPopupMessage.clear();
    }
    return true;
}

void EditorMapCannonSalvoScene::drawBlockingPopup(void) const
{
    if (!this->blockingPopupVisible || this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    const SDL_FRect screenRect = GetGameScreen().rect;
    const float popupW = (std::min)(760.0f, screenRect.w - 40.0f);
    const float popupH = (std::min)(270.0f, screenRect.h - 40.0f);
    const SDL_FRect overlayRect{
        screenRect.x,
        screenRect.y,
        screenRect.w,
        screenRect.h};
    const SDL_FRect popupRect{
        screenRect.x + ((screenRect.w - popupW) * 0.5f),
        screenRect.y + ((screenRect.h - popupH) * 0.5f),
        popupW,
        popupH};
    const SDL_FRect okButtonRect{
        popupRect.x + popupRect.w - 124.0f,
        popupRect.y + popupRect.h - 52.0f,
        104.0f,
        32.0f};
    const SDL_FRect messageRect{
        popupRect.x + 16.0f,
        popupRect.y + 56.0f,
        popupRect.w - 32.0f,
        popupRect.h - 118.0f};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 170});
    rc2d_graphics_rectangle("fill", &overlayRect);
    fillAndOutlineRect(popupRect, RC2D_Color{20, 28, 40, 248}, RC2D_Color{215, 177, 93, 255});
    drawTextAt(const_cast<RC2D_Font*>(&this->overlayFont), "Import JSON impossible", popupRect.x + 16.0f, popupRect.y + 16.0f, RC2D_Color{245, 227, 174, 255});
    drawWrappedText(const_cast<RC2D_Font*>(&this->overlayFont), this->blockingPopupMessage, messageRect, RC2D_Color{220, 227, 236, 255}, 2.0f);
    fillAndOutlineRect(okButtonRect, RC2D_Color{82, 61, 28, 246}, RC2D_Color{232, 206, 138, 255});
    drawCenteredText(const_cast<RC2D_Font*>(&this->overlayFont), "OK", okButtonRect, RC2D_Color{255, 247, 220, 255});
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void EditorMapCannonSalvoScene::openImportFolderDialog(ImportTarget target)
{
    {
        std::lock_guard<std::mutex> lock(this->pendingFolderMutex);
        this->pendingFolderDialogCompleted = false;
        this->pendingFolderDialogCanceled = false;
        this->pendingImportTarget = target;
        this->pendingFolderAbsolute.clear();
    }

    RC2D_FileDialogOptions options{};
    options.window = rc2d_window_getWindow();
    const bool jsonFileMode =
        (target == ImportTarget::PROJECTILE_GLOW_JSON ||
         target == ImportTarget::PROJECTILE_GLOW_JSON_EXPORT ||
         target == ImportTarget::TRAIL_JSON_IMPORT ||
         target == ImportTarget::TRAIL_JSON_EXPORT);
    const bool jsonSaveMode =
        (target == ImportTarget::PROJECTILE_GLOW_JSON_EXPORT ||
         target == ImportTarget::TRAIL_JSON_EXPORT);
    options.filters = jsonFileMode ? kJsonFileFilters.data() : kFolderFilters.data();
    options.num_filters = jsonFileMode ? static_cast<int>(kJsonFileFilters.size()) : static_cast<int>(kFolderFilters.size());
    std::string defaultJsonLocation{};
    if (jsonSaveMode)
    {
        std::error_code fsError;
        const std::filesystem::path defaultSavePath =
            (target == ImportTarget::TRAIL_JSON_EXPORT)
                ? std::filesystem::path("assets/data/ammo-trail-.json")
                : std::filesystem::path("assets/data/ammo-illu-.json");
        defaultJsonLocation = std::filesystem::absolute(defaultSavePath, fsError).string();
        if (fsError)
        {
            defaultJsonLocation.clear();
        }
        defaultJsonLocation = normalizePathSlashes(defaultJsonLocation);
    }
    options.default_location =
        (!defaultJsonLocation.empty()) ? defaultJsonLocation.c_str() : nullptr;
    options.allow_many = false;
    options.title = jsonSaveMode
        ? ((target == ImportTarget::TRAIL_JSON_EXPORT)
               ? "Exporter la configuration trail projectile en JSON"
               : "Exporter la configuration glow projectile en JSON")
        : (jsonFileMode
            ? ((target == ImportTarget::TRAIL_JSON_IMPORT)
                   ? "Selectionner un JSON trail projectile dans assets/data"
                   : "Selectionner un JSON glow projectile dans assets/data")
            : "Importer un dossier asset pour la salve");
    options.accept_label = jsonSaveMode ? "Enregistrer" : (jsonFileMode ? "Choisir" : "Importer");
    options.cancel_label = "Annuler";
    if (jsonSaveMode)
    {
        rc2d_filedialog_saveFile(&EditorMapCannonSalvoScene::onImportFolderDialogResult, this, &options);
    }
    else if (jsonFileMode)
    {
        rc2d_filedialog_openFile(&EditorMapCannonSalvoScene::onImportFolderDialogResult, this, &options);
    }
    else
    {
        rc2d_filedialog_openFolder(&EditorMapCannonSalvoScene::onImportFolderDialogResult, this, &options);
    }
}

void EditorMapCannonSalvoScene::processPendingFolderRequest(void)
{
    bool hasResult = false;
    bool canceled = false;
    ImportTarget target = ImportTarget::NONE;
    std::string selectedFolder;
    {
        std::lock_guard<std::mutex> lock(this->pendingFolderMutex);
        hasResult = this->pendingFolderDialogCompleted;
        if (hasResult)
        {
            canceled = this->pendingFolderDialogCanceled;
            target = this->pendingImportTarget;
            selectedFolder = this->pendingFolderAbsolute;
            this->pendingFolderDialogCompleted = false;
            this->pendingFolderDialogCanceled = false;
            this->pendingImportTarget = ImportTarget::NONE;
            this->pendingFolderAbsolute.clear();
        }
    }

    if (!hasResult)
    {
        return;
    }
    if (canceled)
    {
        this->statusMessage = "Import annule.";
        return;
    }

    if (!this->appendImportedFolder(target, selectedFolder))
    {
        if (target == ImportTarget::PROJECTILE_GLOW_JSON ||
            target == ImportTarget::TRAIL_JSON_IMPORT)
        {
            const std::string popupText =
                "Le fichier n'a pas pu etre charge.\n\n"
                "Verifie que le JSON est dans le dossier assets runtime (a cote de l'executable), "
                "et qu'il est valide pour ce type d'import.\n\n"
                "Chemin selectionne:\n" + shortenMiddle(selectedFolder, 96U);
            this->showBlockingPopup(popupText);
            this->statusMessage =
                "Import JSON refuse: hors assets runtime ou JSON invalide.";
        }
        else
        {
            this->statusMessage = "Fichier invalide ou hors assets: " + selectedFolder;
        }
    }
}

bool EditorMapCannonSalvoScene::appendImportedFolder(ImportTarget target, const std::string& folderAbsolutePath)
{
    if (target == ImportTarget::PROJECTILE_GLOW_JSON_EXPORT)
    {
        if (this->selectedProjectileIndex < 0 ||
            this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
        {
            return false;
        }

        std::string exportPath = normalizePathSlashes(folderAbsolutePath);
        if (std::filesystem::path(exportPath).extension().generic_string() != ".json")
        {
            exportPath += ".json";
        }

        const VFXClassic::IlluminatedProjectileGlowConfig config =
            VFXClassic::getIlluminatedProjectileGlowConfig();
        const bool exported = VFXClassic::exportIlluminatedProjectileGlowConfigToFile(
            exportPath.c_str(),
            &config);
        if (!exported)
        {
            return false;
        }

        const std::string relativeIfInsideAssets = makeRelativeProjectPath(exportPath);
        if (relativeIfInsideAssets.rfind("assets/", 0U) == 0U)
        {
            ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
            item.illuminatedGlowConfigPath = normalizePathSlashes(relativeIfInsideAssets);
        }

        this->statusMessage =
            "Glow exporte: " + shortenMiddle(exportPath, 58U);
        return true;
    }
    if (target == ImportTarget::TRAIL_JSON_EXPORT)
    {
        std::string exportPath = normalizePathSlashes(folderAbsolutePath);
        if (std::filesystem::path(exportPath).extension().generic_string() != ".json")
        {
            exportPath += ".json";
        }

        const bool exported =
            MaritimeCannonSalvoSystem::exportProjectileTrajectoryTuningsToFile(exportPath.c_str());
        if (!exported)
        {
            return false;
        }

        this->statusMessage =
            "Trail exporte: " + shortenMiddle(exportPath, 58U);
        return true;
    }
    if (target == ImportTarget::TRAIL_JSON_IMPORT)
    {
        if (this->selectedProjectileIndex < 0 ||
            this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
        {
            return false;
        }

        const std::string relative = makeRelativeProjectPath(folderAbsolutePath);
        if (relative.rfind("assets/", 0U) != 0U)
        {
            return false;
        }
        if (std::filesystem::path(relative).extension().generic_string() != ".json")
        {
            return false;
        }

        MaritimeCannonSalvoSystem::ProjectileRibbonTrailConfig loadedConfig{};
        const bool loaded = MaritimeCannonSalvoSystem::readProjectileRibbonTrailConfigFromFile(
            relative.c_str(),
            &loadedConfig);
        if (!loaded)
        {
            return false;
        }

        MaritimeCannonSalvoSystem::setProjectileRibbonTrailConfig(loadedConfig);
        ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
        item.ribbonTrailConfig = loadedConfig;
        item.ribbonTrailConfigInitialized = true;
        item.ribbonTrailConfigPath = normalizePathSlashes(relative);
        this->statusMessage = "JSON trail importe: " + shortenMiddle(relative, 58U);
        return true;
    }

    const std::string relative = makeRelativeProjectPath(folderAbsolutePath);
    if (relative.rfind("assets/", 0U) != 0U)
    {
        return false;
    }
    if (target == ImportTarget::PROJECTILE_GLOW_JSON)
    {
        if (this->selectedProjectileIndex < 0 ||
            this->selectedProjectileIndex >= static_cast<int>(this->projectileOptions.size()))
        {
            return false;
        }
        if (std::filesystem::path(relative).extension().generic_string() != ".json")
        {
            return false;
        }

        ListItem& item = this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
        VFXClassic::IlluminatedProjectileGlowConfig loadedConfig{};
        const bool loaded = VFXClassic::readIlluminatedProjectileGlowConfigFromFile(
            relative.c_str(),
            &loadedConfig);
        if (!loaded)
        {
            return false;
        }

        item.illuminatedGlowConfigPath = normalizePathSlashes(relative);
        item.projectileGlowMode = ProjectileGlowMode::LIVE;
        item.illuminatedGlowConfig = loadedConfig;
        item.illuminatedGlowConfigInitialized = true;
        VFXClassic::setIlluminatedProjectileGlowConfig(loadedConfig);
        if (this->attackerShip.areSpritesLoaded() && this->targetShip.areSpritesLoaded())
        {
            GetMaritimeCannonSalvoSystem().clear();
            this->clearLocalVfxShips();
            this->fireCurrentSalvo();
            this->salvoTimerSec = 0.0f;
        }
        this->statusMessage = "JSON glow importe (mode live): " + shortenMiddle(relative, 58U);
        return true;
    }

    const std::filesystem::path fsPath(relative);
    const std::string folderName = fsPath.filename().generic_string();

    switch (target)
    {
        case ImportTarget::PROJECTILE:
            if (!startsWith(folderName, "vfx-ammo-") || !isLikelyVfxClassicFolder(fsPath))
            {
                return false;
            }
            pushUniqueFolder(&this->projectileOptions, relative);
            (void)this->selectProjectileAtIndex(static_cast<int>(this->projectileOptions.size()) - 1);
            this->statusMessage = "Spritesheet boulet importee: " + relative;
            return true;
        case ImportTarget::TRAIL:
            if (!isLikelyVfxClassicFolder(fsPath))
            {
                return false;
            }
            pushUniqueFolder(&this->trailOptions, relative);
            (void)this->selectTrailAtIndex(static_cast<int>(this->trailOptions.size()) - 1);
            this->statusMessage = "Spritesheet trail importee: " + relative;
            return true;
        case ImportTarget::START_VFX:
            if (startsWith(folderName, "vfx-ammo-") || !isLikelyVfxShipFolder(fsPath))
            {
                return false;
            }
            pushUniqueFolder(&this->startVfxOptions, relative);
            this->selectedStartVfxIndex = static_cast<int>(this->startVfxOptions.size()) - 1;
            this->statusMessage = "VFX depart importe: " + relative;
            return true;
        case ImportTarget::END_VFX:
            if (startsWith(folderName, "vfx-ammo-") || !isLikelyVfxShipFolder(fsPath))
            {
                return false;
            }
            pushUniqueFolder(&this->endVfxOptions, relative);
            this->focusedEndVfxIndex = static_cast<int>(this->endVfxOptions.size()) - 1;
            if (this->focusedEndVfxIndex >= 0)
            {
                this->endVfxOptions[static_cast<std::size_t>(this->focusedEndVfxIndex)].selected = true;
                this->refreshEndDelayInputFromFocus();
            }
            this->statusMessage = "VFX impact importe: " + relative;
            return true;
        case ImportTarget::NONE:
        default:
            return false;
    }
}

void EditorMapCannonSalvoScene::drawHud(void) const
{
    if (this->overlayFont.sdl_font == nullptr)
    {
        return;
    }

    const ListItem* selectedProjectile =
        (this->selectedProjectileIndex >= 0 &&
         this->selectedProjectileIndex < static_cast<int>(this->projectileOptions.size()))
            ? &this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)]
            : nullptr;

    this->drawToolbarButton(this->buttonDebugPanelRect, "PANNEAU BOULETS", this->illuminatedProjectileDebugPanel.isVisible());
    this->drawToolbarButton(this->buttonListsVisibilityRect, this->showLists ? "LISTES ON" : "LISTES OFF", this->showLists);
    this->drawToolbarButton(this->buttonOceanPrevRect, "OCEAN -", false);
    this->drawToolbarButton(this->buttonOceanNextRect, "OCEAN +", false);
    this->drawToolbarButton(this->buttonCenterAttackerRect, "CENTRER TIREUR", false);
    this->drawToolbarButton(this->buttonZoomOutRect, "ZOOM -", false);
    this->drawToolbarButton(this->buttonZoomInRect, "ZOOM +", false);
    this->drawToolbarButton(this->buttonSalvoOneRect, "SALVE 1 BOULET", this->selectedSalvoBallCount == 1);
    this->drawToolbarButton(this->buttonSalvoFiveRect, "SALVE 5 BOULETS", this->selectedSalvoBallCount == 5);
    this->drawToolbarButton(this->buttonSalvoTenRect, "SALVE 10 BOULETS", this->selectedSalvoBallCount == 10);
    this->drawToolbarButton(this->buttonShipSpeedDownRect, "VITESSE NAVIRES -", false);
    this->drawToolbarButton(this->buttonShipSpeedUpRect, "VITESSE NAVIRES +", false);
    this->drawToolbarButton(this->buttonCadenceDownRect, "CADENCE ATTACK -", false);
    this->drawToolbarButton(this->buttonCadenceUpRect, "CADENCE ATTACK +", false);
    this->drawToolbarButton(
        this->buttonProjectileIlluminatedRect,
        (selectedProjectile != nullptr && selectedProjectile->illuminatedEnabled)
            ? "BOULET ILLUMINE ACTIVE"
            : "BOULET ILLUMINE INACTIF",
        selectedProjectile != nullptr && selectedProjectile->illuminatedEnabled);
    this->drawToolbarButton(
        this->buttonProjectileTrailRect,
        (selectedProjectile != nullptr && selectedProjectile->ribbonTrailEnabled)
            ? "RIBBON TRAIL ACTIVE"
            : "RIBBON TRAIL INACTIF",
        selectedProjectile != nullptr && selectedProjectile->ribbonTrailEnabled);
    this->drawToolbarButton(
        this->buttonProjectileGlowModeRect,
        (selectedProjectile != nullptr)
            ? projectileGlowModeLabel(selectedProjectile->projectileGlowMode)
            : "MODE GLOW LIVE",
        selectedProjectile != nullptr &&
            selectedProjectile->projectileGlowMode != ProjectileGlowMode::LIVE);
    this->drawDelayInput();

    if (this->showLists)
    {
        this->drawListPanel(
            this->shipListRect,
            "Navires assets/images/ships",
            this->shipOptions,
            this->selectedShipIndex,
            this->shipListScrollOffset,
            this->shipListScrollDragActive,
            false,
            false,
            nullptr,
            nullptr);
        this->drawListPanel(
            this->projectileListRect,
            "Boulets assets/images/vfxclassic",
            this->projectileOptions,
            this->selectedProjectileIndex,
            this->projectileListScrollOffset,
            this->projectileListScrollDragActive,
            false,
            false,
            &this->projectileImportButtonRect,
            "IMPORTER SPRITESHEET BOULETS");
        this->drawListPanel(
            this->trailListRect,
            "Trail VFX assets/images/vfxclassic",
            this->trailOptions,
            this->selectedTrailIndex,
            this->trailListScrollOffset,
            this->trailListScrollDragActive,
            false,
            false,
            &this->trailImportButtonRect,
            "IMPORTER SPRITESHEET TRAIL");
        this->drawListPanel(
            this->startVfxListRect,
            "startActionVfxShipFolderPathForAttacker",
            this->startVfxOptions,
            this->selectedStartVfxIndex,
            this->startVfxListScrollOffset,
            this->startVfxListScrollDragActive,
            false,
            false,
            &this->startVfxImportButtonRect,
            "IMPORTER VFX DEPART");
        this->drawListPanel(
            this->endVfxListRect,
            "endActionVfxShipFoldersPathForTarget",
            this->endVfxOptions,
            -1,
            this->endVfxListScrollOffset,
            this->endVfxListScrollDragActive,
            true,
            true,
            &this->endVfxImportButtonRect,
            "IMPORTER VFX IMPACT");
    }

    char info[640] = {};
    const char* oceanLabel = "?";
    if (this->selectedOceanColorIndex >= 0 &&
        this->selectedOceanColorIndex < static_cast<int>(kOceanColors.size()))
    {
        oceanLabel = kOceanColors[static_cast<std::size_t>(this->selectedOceanColorIndex)].label;
    }
    const std::string statusShort = shortenMiddle(this->statusMessage, 58U);
    const std::string projectileGlowPathShort =
        (selectedProjectile != nullptr &&
         selectedProjectile->projectileGlowMode == ProjectileGlowMode::LIVE)
            ? std::string("mode live")
            : (selectedProjectile != nullptr &&
               selectedProjectile->projectileGlowMode == ProjectileGlowMode::DEFAULT_JSON)
            ? shortenMiddle(VFXClassic::getIlluminatedProjectileGlowConfigPath(), 48U)
            : (selectedProjectile != nullptr && !selectedProjectile->illuminatedGlowConfigPath.empty())
            ? shortenMiddle(selectedProjectile->illuminatedGlowConfigPath, 48U)
            : std::string("aucun json glow");
    const std::string trailPathShort =
        (selectedProjectile != nullptr && !selectedProjectile->ribbonTrailVfxFolderPath.empty())
            ? shortenMiddle(selectedProjectile->ribbonTrailVfxFolderPath, 44U)
            : std::string("aucun trail vfx");
    SDL_snprintf(
        info,
        sizeof(info),
        "F7 editeur salves | gauche=attaquant, droite=cible | salve %d boulet(s) | cadence %.2fs | ocean %s | glow %s | trail %s | %s",
        this->selectedSalvoBallCount,
        this->salvoIntervalSec,
        oceanLabel,
        projectileGlowPathShort.c_str(),
        trailPathShort.c_str(),
        statusShort.c_str());
    const SDL_FRect screenRect = GetGameScreen().rect;
    const float textX = screenRect.x + 10.0f;
    this->drawTextLine(info, textX, screenRect.y + 4.0f, kHudStatusColor);
}

void EditorMapCannonSalvoScene::unload(void)
{
    if (gActiveCannonSalvoAttackerShip == &this->attackerShip)
    {
        gActiveCannonSalvoAttackerShip = nullptr;
    }
    if (gActiveCannonSalvoTargetShip == &this->targetShip)
    {
        gActiveCannonSalvoTargetShip = nullptr;
    }
    EditorMapSceneLayout::popBottomToolbarPlayfieldMargins();
    GetMaritimeCannonSalvoSystem().clear();
    this->clearLocalVfxShips();
    this->clickMarker.hide();
    this->attackerShip.unloadSprites();
    this->targetShip.unloadSprites();
    this->scrollBarOverlay.unload();
    this->illuminatedProjectileDebugPanel.unload();
    if (this->editorTextInputEnabled)
    {
        rc2d_keyboard_setTextInput(false);
        this->editorTextInputEnabled = false;
    }
    ResetStorageFontRef(&this->overlayFont);
    this->backgroundWidget.unload();
    GetOceanShader().unload();
}

void EditorMapCannonSalvoScene::load(void)
{
    gActiveCannonSalvoAttackerShip = &this->attackerShip;
    gActiveCannonSalvoTargetShip = &this->targetShip;
    EditorMapSceneLayout::pushBottomToolbarPlayfieldMargins();
    this->resetEditorState();
    this->backgroundWidget.load();
    this->overlayFont = OpenStorageFont(
        "assets/fonts/TradeWinds-Regular.ttf",
        RC2D_STORAGE_TITLE,
        15.0f);
    this->scrollBarOverlay.load();
    this->clickMarker.setDurationSeconds(0.85);
    this->clickMarker.hide();
    this->illuminatedProjectileDebugPanel.load();
    this->illuminatedProjectileDebugPanel.setVisible(false);

    GetMaritimeCannonSalvoSystem().clear();
    this->clearLocalVfxShips();
    (void)MaritimeCannonSalvoSystem::loadProjectileTrajectoryTuningsFromFile();
    this->collectAssetLists();

    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();
    map.update();
    camera.setZoomFactor(0.40f);
    const SDL_Point center = map.sectorToTile(Map::NUM_SECTORS_X / 2, Map::NUM_SECTORS_Y / 2);
    camera.centerCameraOnTile(
        static_cast<float>(center.x),
        static_cast<float>(center.y),
        map,
        map.rect);
    camera.update(map, map.rect);
    this->applySelectedOceanColor();
    this->updateToolbarLayout();
    (void)this->selectShipAtIndex(this->selectedShipIndex);
    (void)this->selectProjectileAtIndex(this->selectedProjectileIndex);

    this->statusMessage = "Salve maritime locale: aucun VFX n'est pousse dans GameState.";
}

void EditorMapCannonSalvoScene::update(double dt)
{
    Map& map = GetCurrentMap();
    Camera& camera = GetCamera();

    map.update();
    this->updateToolbarLayout();
    this->processPendingFolderRequest();
    this->applyPendingOceanColorStep();
    this->scrollBarOverlay.update(dt, camera, map, map.rect);
    (void)updateEditorCameraKeyboardScroll(dt, camera, map, map.rect);
    this->clickMarker.update(dt);
    GetOceanShader().update(dt, true);
    if (this->scrollBarOverlay.isInteracting())
    {
        this->miniMapDragActive = false;
    }
    this->handleMiniMapDragFromMouse();
    if (this->showLists)
    {
        this->handleListPanelScrollDragFromMouse(
            this->shipListRect,
            false,
            static_cast<int>(this->shipOptions.size()),
            &this->shipListScrollOffset,
            &this->shipListScrollDragActive,
            &this->shipListScrollDragGrabOffsetY);
        this->handleListPanelScrollDragFromMouse(
            this->projectileListRect,
            true,
            static_cast<int>(this->projectileOptions.size()),
            &this->projectileListScrollOffset,
            &this->projectileListScrollDragActive,
            &this->projectileListScrollDragGrabOffsetY);
        this->handleListPanelScrollDragFromMouse(
            this->trailListRect,
            true,
            static_cast<int>(this->trailOptions.size()),
            &this->trailListScrollOffset,
            &this->trailListScrollDragActive,
            &this->trailListScrollDragGrabOffsetY);
        this->handleListPanelScrollDragFromMouse(
            this->startVfxListRect,
            true,
            static_cast<int>(this->startVfxOptions.size()),
            &this->startVfxListScrollOffset,
            &this->startVfxListScrollDragActive,
            &this->startVfxListScrollDragGrabOffsetY);
        this->handleListPanelScrollDragFromMouse(
            this->endVfxListRect,
            true,
            static_cast<int>(this->endVfxOptions.size()),
            &this->endVfxListScrollOffset,
            &this->endVfxListScrollDragActive,
            &this->endVfxListScrollDragGrabOffsetY);
    }
    else
    {
        this->shipListScrollDragActive = false;
        this->projectileListScrollDragActive = false;
        this->trailListScrollDragActive = false;
        this->startVfxListScrollDragActive = false;
        this->endVfxListScrollDragActive = false;
    }

    this->attackerShip.update(dt, map);
    this->targetShip.update(dt, map);
    GetMaritimeCannonSalvoSystem().update(dt);

    const float dtf = (std::isfinite(dt) && dt > 0.0) ? static_cast<float>(dt) : 0.0f;
    const float interval = (std::max)(0.05f, this->salvoIntervalSec);
    this->salvoTimerSec += dtf;
    while (this->salvoTimerSec >= interval)
    {
        this->salvoTimerSec -= interval;
        this->fireCurrentSalvo();
    }

    this->illuminatedProjectileDebugPanel.update(dt);
    if (this->selectedProjectileIndex >= 0 &&
        this->selectedProjectileIndex < static_cast<int>(this->projectileOptions.size()))
    {
        ListItem& selectedProjectile =
            this->projectileOptions[static_cast<std::size_t>(this->selectedProjectileIndex)];
        if (gEditorGlowLiveClearSourceRequested &&
            selectedProjectile.projectileGlowMode == ProjectileGlowMode::LIVE)
        {
            selectedProjectile.illuminatedGlowConfigPath.clear();
            gEditorGlowLiveClearSourceRequested = false;
        }
        if (gEditorTrailClearSourceRequested)
        {
            selectedProjectile.ribbonTrailConfigPath.clear();
            gEditorTrailClearSourceRequested = false;
        }
        gEditorGlowModeDefault =
            (selectedProjectile.projectileGlowMode == ProjectileGlowMode::DEFAULT_JSON);
        gEditorGlowLiveSourcePath =
            (!gEditorGlowModeDefault && !selectedProjectile.illuminatedGlowConfigPath.empty())
                ? selectedProjectile.illuminatedGlowConfigPath
                : std::string{};
        gEditorTrailSourcePath = !selectedProjectile.ribbonTrailConfigPath.empty()
            ? selectedProjectile.ribbonTrailConfigPath
            : std::string{};
    }
    else
    {
        gEditorGlowModeDefault = false;
        gEditorGlowLiveSourcePath.clear();
        gEditorTrailSourcePath.clear();
    }
    const IlluminatedProjectileDebugPanel::DialogRequest panelDialogRequest =
        this->illuminatedProjectileDebugPanel.consumeDialogRequest();
    if (panelDialogRequest == IlluminatedProjectileDebugPanel::DialogRequest::GLOW_IMPORT_JSON)
    {
        this->openImportFolderDialog(ImportTarget::PROJECTILE_GLOW_JSON);
    }
    else if (panelDialogRequest == IlluminatedProjectileDebugPanel::DialogRequest::GLOW_EXPORT_JSON)
    {
        this->openImportFolderDialog(ImportTarget::PROJECTILE_GLOW_JSON_EXPORT);
    }
    else if (panelDialogRequest == IlluminatedProjectileDebugPanel::DialogRequest::TRAIL_IMPORT_JSON)
    {
        this->openImportFolderDialog(ImportTarget::TRAIL_JSON_IMPORT);
    }
    else if (panelDialogRequest == IlluminatedProjectileDebugPanel::DialogRequest::TRAIL_EXPORT_JSON)
    {
        this->openImportFolderDialog(ImportTarget::TRAIL_JSON_EXPORT);
    }
    (void)this->syncSelectedProjectileGlowConfigFromPanel();
    camera.update(map, map.rect);
    this->syncEditorTextInputState();
}

void EditorMapCannonSalvoScene::draw(void)
{
    Map& map = GetCurrentMap();
    this->backgroundWidget.draw();

    SDL_Renderer* renderer = WorldRenderClip::begin(map.rect);
    if (GetOceanShader().isReady())
    {
        GetOceanShader().draw(map.rect);
    }

    this->clickMarker.draw(map);
    this->updateLocalVfxShipSlotsForDraw();
    this->drawLocalVfxShipSlots(true);
    this->targetShip.draw(map);
    this->attackerShip.draw(map);
    this->drawLocalVfxShipSlots(false);
    GetMaritimeCannonSalvoSystem().drawSalvoProjectiles();
    this->scrollBarOverlay.draw(map.rect, map);
    WorldRenderClip::end(renderer);

    this->drawMiniMap();
    this->drawHud();
    this->illuminatedProjectileDebugPanel.draw();
    this->drawBlockingPopup();
}

void EditorMapCannonSalvoScene::textinput(const RC2D_TextInputEventInfo* info)
{
    if (!this->endDelayInputFocused || info == nullptr || info->text == nullptr || info->text[0] == '\0')
    {
        return;
    }

    for (const char* cursor = info->text; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = static_cast<unsigned char>(*cursor);
        if (!std::isdigit(ch))
        {
            continue;
        }
        if (this->endDelayInputBuffer == "0")
        {
            this->endDelayInputBuffer.clear();
        }
        if (this->endDelayInputBuffer.size() < 10U)
        {
            this->endDelayInputBuffer.push_back(static_cast<char>(ch));
        }
    }

    (void)this->applyFocusedEndVfxDelayInput();
}

void EditorMapCannonSalvoScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)keycode;
    (void)mod;
    (void)keyboardID;

    if (this->blockingPopupVisible)
    {
        if (!isrepeat && (scancode == SDL_SCANCODE_ESCAPE || scancode == SDL_SCANCODE_RETURN))
        {
            this->blockingPopupVisible = false;
            this->blockingPopupMessage.clear();
        }
        return;
    }

    if (this->handleEndDelayInputKey(key, scancode, keycode, isrepeat))
    {
        return;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_F2)
    {
        this->illuminatedProjectileDebugPanel.toggleVisibility();
        return;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_ESCAPE)
    {
        this->endDelayInputFocused = false;
        this->syncEditorTextInputState();
        return;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_LEFTBRACKET)
    {
        this->salvoIntervalSec = (std::clamp)(this->salvoIntervalSec + 0.25f, 0.25f, 20.0f);
        return;
    }
    if (!isrepeat && scancode == SDL_SCANCODE_RIGHTBRACKET)
    {
        this->salvoIntervalSec = (std::clamp)(this->salvoIntervalSec - 0.25f, 0.25f, 20.0f);
        return;
    }
}

void EditorMapCannonSalvoScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    if (this->handleBlockingPopupClick(x, y, button))
    {
        return;
    }
    if (this->illuminatedProjectileDebugPanel.mousepressed(x, y, button))
    {
        return;
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT && this->endDelayInputFocused && !this->pointInRect(x, y, this->endDelayInputRect))
    {
        (void)this->applyFocusedEndVfxDelayInput();
        this->endDelayInputFocused = false;
        this->syncEditorTextInputState();
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT && this->handleToolbarClick(x, y))
    {
        return;
    }
    if (this->handleMiniMapClick(x, y, button))
    {
        return;
    }
    if (button == RC2D_MOUSE_BUTTON_LEFT && this->handleAssetListClick(x, y))
    {
        return;
    }

    Map& map = GetCurrentMap();
    if (button == RC2D_MOUSE_BUTTON_LEFT && this->scrollBarOverlay.handleClick(x, y, map.rect))
    {
        return;
    }
    (void)this->handleMapClick(x, y, button);
}

void EditorMapCannonSalvoScene::mousewheelmoved(
    RC2D_MouseWheelDirection direction,
    float x,
    float y,
    Sint32 integer_x,
    Sint32 integer_y,
    float mouse_x,
    float mouse_y,
    SDL_MouseID mouseID)
{
    (void)x;
    (void)y;
    (void)integer_x;
    (void)mouseID;

    if (this->blockingPopupVisible)
    {
        return;
    }

    if (this->illuminatedProjectileDebugPanel.mousewheelmoved(
            direction,
            x,
            y,
            integer_x,
            integer_y,
            mouse_x,
            mouse_y,
            mouseID))
    {
        return;
    }

    int delta = integer_y;
    if (delta == 0)
    {
        delta = (direction == RC2D_SCROLL_UP) ? 1 : ((direction == RC2D_SCROLL_DOWN) ? -1 : 0);
    }
    if (delta == 0)
    {
        return;
    }

    float renderX = 0.0f;
    float renderY = 0.0f;
    if (!this->getMouseRenderPosition(&renderX, &renderY))
    {
        renderX = mouse_x;
        renderY = mouse_y;
        if (std::isfinite(mouse_x) && std::isfinite(mouse_y))
        {
            this->convertWindowToRender(mouse_x, mouse_y, &renderX, &renderY);
        }
    }

    const int step = (std::max)(1, std::abs(delta));
    if (this->handleListMouseWheel(renderX, renderY, (delta > 0) ? step : -step))
    {
        return;
    }

    if (this->pointInRect(renderX, renderY, this->miniMapRect))
    {
        return;
    }

    this->adjustMapZoom((delta > 0) ? 0.05f : -0.05f);
}

bool EditorMapCannonSalvoScene::pointInRect(float x, float y, const SDL_FRect& rect) const
{
    return (
        rect.w > 0.0f &&
        rect.h > 0.0f &&
        x >= rect.x &&
        x <= rect.x + rect.w &&
        y >= rect.y &&
        y <= rect.y + rect.h);
}

void EditorMapCannonSalvoScene::convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const
{
    if (outX == nullptr || outY == nullptr)
    {
        return;
    }

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

bool EditorMapCannonSalvoScene::getMouseRenderPosition(float* outX, float* outY) const
{
    if (outX == nullptr || outY == nullptr)
    {
        return false;
    }

    float windowX = 0.0f;
    float windowY = 0.0f;
    rc2d_mouse_getPosition(&windowX, &windowY);
    this->convertWindowToRender(windowX, windowY, outX, outY);
    return true;
}

void EditorMapCannonSalvoScene::onImportFolderDialogResult(void* userdata, const char* const* filelist, int filter_index)
{
    (void)filter_index;
    EditorMapCannonSalvoScene* scene = static_cast<EditorMapCannonSalvoScene*>(userdata);
    if (scene == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(scene->pendingFolderMutex);
    scene->pendingFolderDialogCompleted = true;
    if (filelist == nullptr || filelist[0] == nullptr || filelist[0][0] == '\0')
    {
        scene->pendingFolderDialogCanceled = true;
        scene->pendingFolderAbsolute.clear();
        return;
    }

    scene->pendingFolderDialogCanceled = false;
    scene->pendingFolderAbsolute = filelist[0];
}

#endif // GAME_ENV_DEV

