#include "game/ui/hud/game-settings-widget.h"

#include "game/assets/title-asset-cache.h"
#include "game/map/map.h"
#include "game/ui/text-input-shortcuts.h"

#include "core/context.h"

#include <RC2D/RC2D_system.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

static constexpr float kRefW = 860.0f;
static constexpr float kRefH = 828.0f;

static constexpr float kTopBarHeight = 34.0f;
static constexpr float kSectionGap = 8.0f;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;
static constexpr float kLanguageRowHeight = 34.0f;
static constexpr float kHudScalePanelHeight = 280.0f;
static constexpr float kHudScaleRowHeight = 26.0f;
static constexpr float kHudScaleRowGap = 6.0f;
static constexpr float kHudScaleTrackHeight = 8.0f;
static constexpr float kHudScaleThumbWidth = 14.0f;
static constexpr float kHudScaleThumbHeight = 22.0f;
static constexpr float kHudScaleToggleButtonWidth = 86.0f;
static constexpr float kHudScaleToggleButtonHeight = 24.0f;
static constexpr float kHudScaleValueLabelWidth = 48.0f;
static constexpr float kHudScaleTrackLeftOffset = 232.0f;
static constexpr float kHudScaleRowsTopOffset = 70.0f;
static constexpr float kMapFramePanelHeight = 170.0f;
static constexpr float kMapFrameRowsTopOffset = 78.0f;
static constexpr float kMapFrameRowHeight = 26.0f;
static constexpr float kMapFrameRowGap = 6.0f;
static constexpr int kMapFrameMarginMaxPct = 20;
static constexpr float kHudScaleMinValueDefault = 0.50f;
static constexpr float kHudScaleMinValueBars = 0.75f;
static constexpr float kHudScaleMaxValue = 1.0f;
static constexpr float kHudScaleStepValue = 0.05f;
static constexpr float kControlsViewportPadding = 12.0f;
static constexpr float kControlsFooterHeight = 44.0f;
static constexpr float kControlsSectionHeight = 30.0f;
static constexpr float kControlsActionRowHeight = 38.0f;
static constexpr float kControlsSpeedRowHeight = 48.0f;
static constexpr float kControlsRowGap = 4.0f;
static constexpr float kControlsKeyButtonWidth = 156.0f;
static constexpr float kControlsResetButtonWidth = 128.0f;
static constexpr float kControlsButtonHeight = 26.0f;
static constexpr float kControlsTrackHeight = 8.0f;
static constexpr float kControlsThumbWidth = 14.0f;
static constexpr float kControlsThumbHeight = 22.0f;
static constexpr float kControlsWheelStep = 52.0f;
static constexpr float kGraphicsViewportPadding = 12.0f;
static constexpr float kGraphicsWheelStep = 52.0f;
static constexpr float kGraphicsSectionGeneralHeight = 112.0f;
static constexpr float kGraphicsSectionWindowHeight = 264.0f;
static constexpr float kGraphicsSectionInterfaceHeight = 112.0f;
static constexpr float kGraphicsSectionAnimationsHeight = 176.0f;
static constexpr float kGraphicsSectionGameplayHeight = 112.0f;
static constexpr float kGraphicsOptionRowHeight = 34.0f;
static constexpr float kGraphicsCheckboxSize = 24.0f;
static constexpr float kGraphicsSelectWidth = 340.0f;
static constexpr float kGraphicsSelectHeight = 34.0f;
static constexpr float kCameraScrollSpeedMinSectors = 2.0f;
static constexpr float kCameraScrollSpeedMaxSectors = 16.0f;
static constexpr float kCameraScrollSpeedDefaultSectors = 8.0f;
static constexpr float kCameraScrollSpeedStepSectors = 0.5f;
static constexpr int kMaxVisibleLanguageRows = 6;
static constexpr std::size_t kMaxRedeemCodeLength = 32U;
static constexpr double kCursorBlinkPeriod = 0.55;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 255};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTabInactive = RC2D_Color{26, 20, 14, 238};
static constexpr RC2D_Color kTabActive = RC2D_Color{54, 28, 8, 242};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 236};
static constexpr RC2D_Color kButtonFill = RC2D_Color{45, 35, 27, 238};
static constexpr RC2D_Color kButtonFillActive = RC2D_Color{66, 50, 37, 242};
static constexpr RC2D_Color kButtonFillHover = RC2D_Color{92, 68, 38, 246};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextBody = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kTextMuted = RC2D_Color{126, 132, 142, 255};
static constexpr RC2D_Color kInputSelectionFill = RC2D_Color{67, 96, 144, 215};
static constexpr RC2D_Color kRowLine = RC2D_Color{134, 102, 39, 220};
static constexpr RC2D_Color kScrollTrack = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumb = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kSelectionFill = RC2D_Color{28, 33, 44, 240};
static constexpr RC2D_Color kSliderTrackFill = RC2D_Color{18, 20, 24, 238};
static constexpr RC2D_Color kSliderActiveFill = RC2D_Color{78, 58, 29, 240};
static constexpr RC2D_Color kSliderInactiveFill = RC2D_Color{48, 55, 64, 236};
static constexpr RC2D_Color kSliderThumbFill = RC2D_Color{184, 132, 30, 245};
static constexpr float kScrollThumbWheelHighlightSec = 0.25f;
static constexpr RC2D_Color kSliderThumbBorder = RC2D_Color{227, 210, 153, 255};
static constexpr RC2D_Color kButtonFillMuted = RC2D_Color{28, 31, 36, 236};

static bool g_graphicsFogOfWarEnabled = true;
static bool g_graphicsShipWakeTrailsEnabled = true;

struct GameSettingsLayout {
    SDL_FRect outer;
    SDL_FRect inner;
    SDL_FRect topBar;
    SDL_FRect closeButton;
    std::array<SDL_FRect, 4> tabs;
    SDL_FRect headerDragRect;
    SDL_FRect content;

    SDL_FRect redeemPanel;
    SDL_FRect redeemInput;
    SDL_FRect redeemButton;

    SDL_FRect configuratorPanel;
    SDL_FRect configuratorButton;

    SDL_FRect hudScalePanel;
    std::array<SDL_FRect, static_cast<std::size_t>(GameSettingsWidget::HudScaleTarget::COUNT)> hudScaleRows;
    std::array<SDL_FRect, static_cast<std::size_t>(GameSettingsWidget::HudScaleTarget::COUNT)> hudScaleTracks;
    std::array<SDL_FRect, static_cast<std::size_t>(GameSettingsWidget::HudScaleTarget::COUNT)> hudScaleVisibilityButtons;

    SDL_FRect mapFramePanel;
    std::array<SDL_FRect, 3> mapFrameRows;
    std::array<SDL_FRect, 3> mapFrameTracks;

    SDL_FRect languageRow;
    SDL_FRect languageButton;
    SDL_FRect languageDropdown;
    SDL_FRect languageViewport;
    SDL_FRect languageScrollTrack;

    SDL_FRect placeholderPanel;
    SDL_FRect controlsPanel;
    SDL_FRect controlsViewport;
    SDL_FRect controlsScrollTrack;
    SDL_FRect controlsResetAllButton;
    SDL_FRect controlsConflictPrompt;
    SDL_FRect controlsConflictConfirmButton;
    SDL_FRect controlsConflictCancelButton;

    SDL_FRect graphicsPanel;
    SDL_FRect graphicsViewport;
    SDL_FRect graphicsScrollTrack;
    SDL_FRect graphicsGeneralPanel;
    SDL_FRect graphicsVsyncRow;
    SDL_FRect graphicsVsyncCheckbox;
    SDL_FRect graphicsWindowPanel;
    SDL_FRect graphicsWindowModeRow;
    SDL_FRect graphicsWindowModeButton;
    SDL_FRect graphicsWindowModeDropdown;
    std::array<SDL_FRect, 3> graphicsWindowModeOptions;
    SDL_FRect graphicsPresentationModeRow;
    SDL_FRect graphicsPresentationModeButton;
    SDL_FRect graphicsPresentationModeDropdown;
    std::array<SDL_FRect, 2> graphicsPresentationModeOptions;
    SDL_FRect graphicsMonitorFpsRow;
    SDL_FRect graphicsMonitorFpsBox;
    SDL_FRect graphicsCursorLockRow;
    SDL_FRect graphicsCursorLockCheckbox;
    SDL_FRect graphicsInterfacePanel;
    SDL_FRect graphicsHideCoordinateBackgroundRow;
    SDL_FRect graphicsHideCoordinateBackgroundCheckbox;
    SDL_FRect graphicsAnimationsPanel;
    SDL_FRect graphicsFogOfWarRow;
    SDL_FRect graphicsFogOfWarCheckbox;
    SDL_FRect graphicsShipWakeTrailsRow;
    SDL_FRect graphicsShipWakeTrailsCheckbox;
    SDL_FRect graphicsHideOtherPlayersVfxRow;
    SDL_FRect graphicsHideOtherPlayersVfxCheckbox;
    SDL_FRect graphicsGameplayPanel;
    SDL_FRect graphicsSalvoRow;
    std::array<SDL_FRect, 3> graphicsSalvoOptions;
};

struct GameSettingsLanguageCatalogEntry {
    const char* flagName;
    const char* label;
};

static constexpr std::array<GameSettingsLanguageCatalogEntry, 24> kLanguageCatalog = {{
    {"albania", "Albanais"},
    {"austria", "Allemand (Autriche)"},
    {"belgium", "Francais (Belgique)"},
    {"croatia", "Croate"},
    {"czech_republic", "Tcheque"},
    {"denmark", "Danois"},
    {"england", "Anglais"},
    {"france", "Francais"},
    {"georgia", "Georgien"},
    {"germany", "Allemand"},
    {"hungary", "Hongrois"},
    {"italy", "Italien"},
    {"netherlands", "Neerlandais"},
    {"poland", "Polonais"},
    {"portugal", "Portugais"},
    {"romania", "Roumain"},
    {"scotland", "Anglais (Ecosse)"},
    {"serbia", "Serbe"},
    {"slovakia", "Slovaque"},
    {"slovenia", "Slovene"},
    {"spain", "Espagnol"},
    {"switzerland", "Allemand (Suisse)"},
    {"turkey", "Turc"},
    {"ukraine", "Ukrainien"}
}};

static constexpr std::array<const char*, static_cast<std::size_t>(GameSettingsWidget::HudScaleTarget::COUNT)> kHudScaleLabels = {{
    "Minimap",
    "Barre XP",
    "Barre HP",
    "Zoom map",
    "Centrer navire",
    "Barre d'action"
}};

static constexpr std::array<const char*, 3> kMapFrameMarginRowLabels = {{
    "Marge gauche",
    "Marge droite",
    "Marge bas"
}};

enum class ControlsEntryType : int {
    SECTION = 0,
    ACTION = 1,
    CAMERA_SPEED = 2
};

struct GameSettingsControlsEntry {
    ControlsEntryType type;
    GameSettingsWidget::ControlAction action;
    const char* label;
};

static constexpr std::array<SDL_Scancode, static_cast<std::size_t>(GameSettingsWidget::ControlAction::COUNT)> kDefaultControlScancodes = {{
    SDL_SCANCODE_W,
    SDL_SCANCODE_S,
    SDL_SCANCODE_A,
    SDL_SCANCODE_D,
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_E,
    SDL_SCANCODE_F,
    SDL_SCANCODE_R,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_5,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
    SDL_SCANCODE_9,
    SDL_SCANCODE_J,
    SDL_SCANCODE_LCTRL,
    SDL_SCANCODE_N
}};

static constexpr std::array<GameSettingsControlsEntry, 26> kControlsEntries = {{
    {ControlsEntryType::SECTION, GameSettingsWidget::ControlAction::COUNT, "Contrôles de la caméra"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::CAMERA_MOVE_UP, "Déplacement de la caméra vers le haut"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::CAMERA_MOVE_DOWN, "Déplacement de la caméra vers le bas"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::CAMERA_MOVE_LEFT, "Déplacement de la caméra vers la gauche"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::CAMERA_MOVE_RIGHT, "Déplacement de la caméra vers la droite"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::CENTER_CAMERA_ON_SHIP, "Centrer la caméra sur le navire"},
    {ControlsEntryType::CAMERA_SPEED, GameSettingsWidget::ControlAction::COUNT, "Vitesse de défilement de la caméra"},
    {ControlsEntryType::SECTION, GameSettingsWidget::ControlAction::COUNT, "Contrôles de la barre d'action"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::TOOLBAR_ATTACK, "Attaquer"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::TOOLBAR_CANCEL_ATTACK, "Annuler l'attaque"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::TOOLBAR_BOARDING, "Abordage"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::TOOLBAR_REPAIR, "Réparer"},
    {ControlsEntryType::SECTION, GameSettingsWidget::ControlAction::COUNT, "Touches de raccourci"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_1, "Raccourci 1"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_2, "Raccourci 2"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_3, "Raccourci 3"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_4, "Raccourci 4"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_5, "Raccourci 5"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_6, "Raccourci 6"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_7, "Raccourci 7"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_8, "Raccourci 8"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::SHORTCUT_9, "Raccourci 9"},
    {ControlsEntryType::SECTION, GameSettingsWidget::ControlAction::COUNT, "Other"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::JUMP_MAP, "Jump map"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::FORCE_CLICKED_POSITION_MOVE, "Déplacement forcé de la position cliquée"},
    {ControlsEntryType::ACTION, GameSettingsWidget::ControlAction::TOGGLE_MINIMAP, "Afficher / masquer la minimap"}
}};

static const char* getControlActionDisplayName(GameSettingsWidget::ControlAction action)
{
    for (const GameSettingsControlsEntry& entry : kControlsEntries)
    {
        if (entry.type == ControlsEntryType::ACTION && entry.action == action)
        {
            return entry.label;
        }
    }

    return "cette action";
}

static const char* getGraphicsWindowModeLabel(GameSettingsWidget::GraphicsWindowMode mode)
{
    switch (mode)
    {
        case GameSettingsWidget::GraphicsWindowMode::FULLSCREEN_EXCLUSIVE:
            return "FullScreen Exclusif";
        case GameSettingsWidget::GraphicsWindowMode::FULLSCREEN_BORDERLESS:
            return "FullScreen Borderless";
        case GameSettingsWidget::GraphicsWindowMode::MAXIMIZED_WINDOW:
        default:
            return "Maximized Window";
    }
}

/**
 * @brief Index de ligne du menu deroulant mode fenetre (0..2) -> mode logique.
 */
static GameSettingsWidget::GraphicsWindowMode getGraphicsWindowModeForOptionIndex(std::size_t index)
{
    switch (index)
    {
        case 0U:
            return GameSettingsWidget::GraphicsWindowMode::MAXIMIZED_WINDOW;
        case 1U:
            return GameSettingsWidget::GraphicsWindowMode::FULLSCREEN_EXCLUSIVE;
        case 2U:
            return GameSettingsWidget::GraphicsWindowMode::FULLSCREEN_BORDERLESS;
        default:
            return GameSettingsWidget::GraphicsWindowMode::MAXIMIZED_WINDOW;
    }
}

static const char* getGraphicsPresentationModeLabel(RC2D_LogicalPresentationMode mode)
{
    switch (mode)
    {
        case RC2D_LOGICAL_PRESENTATION_LETTERBOX:
            return "Letterbox";
        case RC2D_LOGICAL_PRESENTATION_OVERSCAN:
        default:
            return "Overscan";
    }
}

static double getDisplayModeRefreshRate(const SDL_DisplayMode* mode)
{
    if (mode == nullptr)
    {
        return 0.0;
    }
    if (mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0)
    {
        return static_cast<double>(mode->refresh_rate_numerator) /
               static_cast<double>(mode->refresh_rate_denominator);
    }
    return (mode->refresh_rate > 0.0f) ? static_cast<double>(mode->refresh_rate) : 0.0;
}

static std::string formatRefreshRate(double refreshRate)
{
    if (refreshRate <= 0.0)
    {
        return "Indisponible";
    }

    char buffer[32] = {};
    const double rounded = std::round(refreshRate);
    if (std::fabs(refreshRate - rounded) < 0.05)
    {
        SDL_snprintf(buffer, sizeof(buffer), "%.0f FPS", rounded);
    }
    else
    {
        SDL_snprintf(buffer, sizeof(buffer), "%.1f FPS", refreshRate);
    }
    return std::string(buffer);
}

static std::string getCurrentMonitorRefreshRateLabel(void)
{
    SDL_Window* window = rc2d_window_getWindow();
    if (window == nullptr)
    {
        return "Indisponible";
    }

    const SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
    if (displayID == 0)
    {
        return "Indisponible";
    }

    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayID);
    const double refreshRate = getDisplayModeRefreshRate(mode);
    return formatRefreshRate(refreshRate);
}

static SDL_FRect getWidgetRectFromGameScreen(void)
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

static SDL_FRect offsetRect(const SDL_FRect& rect, float dx, float dy)
{
    return SDL_FRect{rect.x + dx, rect.y + dy, rect.w, rect.h};
}

static float clampf(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(value, maxValue));
}

static float getHudScaleMinValue(GameSettingsWidget::HudScaleTarget target)
{
    switch (target)
    {
        case GameSettingsWidget::HudScaleTarget::EXPERIENCE_BAR:
        case GameSettingsWidget::HudScaleTarget::HP_BAR:
            return kHudScaleMinValueBars;
        case GameSettingsWidget::HudScaleTarget::MINIMAP:
        case GameSettingsWidget::HudScaleTarget::MAP_ZOOM:
        case GameSettingsWidget::HudScaleTarget::CENTER_SHIP:
        case GameSettingsWidget::HudScaleTarget::ACTION_BAR:
        case GameSettingsWidget::HudScaleTarget::COUNT:
        default:
            return kHudScaleMinValueDefault;
    }
}

static float clampHudScaleValue(GameSettingsWidget::HudScaleTarget target, float value)
{
    return clampf(value, getHudScaleMinValue(target), kHudScaleMaxValue);
}

static float snapHudScaleValue(GameSettingsWidget::HudScaleTarget target, float value)
{
    const float minValue = getHudScaleMinValue(target);
    const float clampedValue = clampHudScaleValue(target, value);
    const float steppedValue =
        minValue +
        (std::round((clampedValue - minValue) / kHudScaleStepValue) * kHudScaleStepValue);
    return clampHudScaleValue(target, steppedValue);
}

static float hudScaleValueToNormalized(GameSettingsWidget::HudScaleTarget target, float value)
{
    const float minValue = getHudScaleMinValue(target);
    const float clampedValue = clampHudScaleValue(target, value);
    const float range = kHudScaleMaxValue - minValue;
    return (range <= 0.0f) ? 0.0f : ((clampedValue - minValue) / range);
}

static float normalizedToHudScaleValue(GameSettingsWidget::HudScaleTarget target, float normalized)
{
    const float minValue = getHudScaleMinValue(target);
    return snapHudScaleValue(
        target,
        minValue + (clampf(normalized, 0.0f, 1.0f) * (kHudScaleMaxValue - minValue)));
}

static float clampCameraScrollSpeed(float value)
{
    return clampf(value, kCameraScrollSpeedMinSectors, kCameraScrollSpeedMaxSectors);
}

static float snapCameraScrollSpeed(float value)
{
    const float clampedValue = clampCameraScrollSpeed(value);
    const float steppedValue =
        kCameraScrollSpeedMinSectors +
        (std::round((clampedValue - kCameraScrollSpeedMinSectors) / kCameraScrollSpeedStepSectors) *
         kCameraScrollSpeedStepSectors);
    return clampCameraScrollSpeed(steppedValue);
}

static float cameraScrollSpeedToNormalized(float value)
{
    const float clampedValue = clampCameraScrollSpeed(value);
    const float range = kCameraScrollSpeedMaxSectors - kCameraScrollSpeedMinSectors;
    return (clampedValue - kCameraScrollSpeedMinSectors) / range;
}

static float normalizedToCameraScrollSpeed(float normalized)
{
    return snapCameraScrollSpeed(
        kCameraScrollSpeedMinSectors +
        (clampf(normalized, 0.0f, 1.0f) * (kCameraScrollSpeedMaxSectors - kCameraScrollSpeedMinSectors)));
}

static int snapMapFrameMarginPercentUi(int value)
{
    return (std::clamp)(value, 0, kMapFrameMarginMaxPct);
}

static float mapFrameMarginPctToNormalized(int pct)
{
    return clampf(
        static_cast<float>(snapMapFrameMarginPercentUi(pct)) / static_cast<float>(kMapFrameMarginMaxPct),
        0.0f,
        1.0f);
}

static int normalizedToMapFrameMarginPct(float normalized)
{
    const float pct = clampf(normalized, 0.0f, 1.0f) * static_cast<float>(kMapFrameMarginMaxPct);
    return snapMapFrameMarginPercentUi(static_cast<int>(std::lround(pct)));
}

static SDL_FRect getMapFrameMarginThumbRect(const SDL_FRect& trackRect, int marginPercent)
{
    const float n = mapFrameMarginPctToNormalized(marginPercent);
    const float thumbCenterX = trackRect.x + (trackRect.w * n);
    return SDL_FRect{
        thumbCenterX - (kHudScaleThumbWidth * 0.5f),
        trackRect.y + ((trackRect.h - kHudScaleThumbHeight) * 0.5f),
        kHudScaleThumbWidth,
        kHudScaleThumbHeight
    };
}

static SDL_FRect getHudScaleThumbRect(
    const SDL_FRect& trackRect,
    GameSettingsWidget::HudScaleTarget target,
    float scaleValue)
{
    const float normalized = hudScaleValueToNormalized(target, scaleValue);
    const float thumbCenterX = trackRect.x + (trackRect.w * normalized);
    return SDL_FRect{
        thumbCenterX - (kHudScaleThumbWidth * 0.5f),
        trackRect.y + ((trackRect.h - kHudScaleThumbHeight) * 0.5f),
        kHudScaleThumbWidth,
        kHudScaleThumbHeight
    };
}

static SDL_FRect getCameraScrollSpeedThumbRect(const SDL_FRect& trackRect, float speedSectors)
{
    const float normalized = cameraScrollSpeedToNormalized(speedSectors);
    const float thumbCenterX = trackRect.x + (trackRect.w * normalized);
    return SDL_FRect{
        thumbCenterX - (kControlsThumbWidth * 0.5f),
        trackRect.y + ((trackRect.h - kControlsThumbHeight) * 0.5f),
        kControlsThumbWidth,
        kControlsThumbHeight
    };
}

static bool isBindableScancode(SDL_Scancode scancode)
{
    return scancode != SDL_SCANCODE_UNKNOWN && scancode != SDL_SCANCODE_ESCAPE;
}

static float getControlsEntryHeight(ControlsEntryType type)
{
    switch (type)
    {
        case ControlsEntryType::SECTION:
            return kControlsSectionHeight;
        case ControlsEntryType::CAMERA_SPEED:
            return kControlsSpeedRowHeight;
        case ControlsEntryType::ACTION:
        default:
            return kControlsActionRowHeight;
    }
}

static float getControlsContentHeight(void)
{
    float height = 0.0f;
    for (std::size_t index = 0; index < kControlsEntries.size(); ++index)
    {
        height += getControlsEntryHeight(kControlsEntries[index].type);
        if (index + 1U < kControlsEntries.size())
        {
            height += kControlsRowGap;
        }
    }
    return height;
}

static float getControlsMaxScrollOffset(const GameSettingsLayout& layout)
{
    return (std::max)(0.0f, getControlsContentHeight() - layout.controlsViewport.h);
}

static SDL_FRect getControlsScrollThumbRect(const GameSettingsLayout& layout, float scrollOffsetY)
{
    const float contentHeight = (std::max)(1.0f, getControlsContentHeight());
    const float maxScrollOffset = getControlsMaxScrollOffset(layout);
    if (maxScrollOffset <= 0.0f)
    {
        return SDL_FRect{
            layout.controlsScrollTrack.x,
            layout.controlsScrollTrack.y,
            layout.controlsScrollTrack.w,
            layout.controlsScrollTrack.h
        };
    }

    const float thumbHeight = (std::max)(
        kMinThumbHeight,
        layout.controlsScrollTrack.h * (layout.controlsViewport.h / contentHeight));
    const float thumbTravel = (std::max)(1.0f, layout.controlsScrollTrack.h - thumbHeight);
    const float scrollRatio = clampf(scrollOffsetY / maxScrollOffset, 0.0f, 1.0f);
    return SDL_FRect{
        layout.controlsScrollTrack.x,
        layout.controlsScrollTrack.y + (thumbTravel * scrollRatio),
        layout.controlsScrollTrack.w,
        thumbHeight
    };
}

static float getGraphicsContentHeight(const GameSettingsLayout& layout)
{
    const float top = layout.graphicsViewport.y;
    const float bottom = layout.graphicsGameplayPanel.y + layout.graphicsGameplayPanel.h;
    return (std::max)(1.0f, bottom - top);
}

static float getGraphicsMaxScrollOffset(const GameSettingsLayout& layout)
{
    const float contentHeight = getGraphicsContentHeight(layout);
    const float viewportHeight = (std::max)(1.0f, layout.graphicsViewport.h);
    return (std::max)(0.0f, contentHeight - viewportHeight);
}

static SDL_FRect getGraphicsScrollThumbRect(const GameSettingsLayout& layout, float scrollOffsetY)
{
    const float contentHeight = (std::max)(1.0f, getGraphicsContentHeight(layout));
    const float viewportHeight = (std::max)(1.0f, layout.graphicsViewport.h);
    const float maxScrollOffset = getGraphicsMaxScrollOffset(layout);
    if (maxScrollOffset <= 0.0f)
    {
        return SDL_FRect{
            layout.graphicsScrollTrack.x,
            layout.graphicsScrollTrack.y,
            layout.graphicsScrollTrack.w,
            layout.graphicsScrollTrack.h
        };
    }

    const float thumbHeight = (std::max)(
        kMinThumbHeight,
        std::floor(layout.graphicsScrollTrack.h * (viewportHeight / contentHeight)));
    const float thumbTravel = (std::max)(1.0f, layout.graphicsScrollTrack.h - thumbHeight);
    const float scrollRatio = clampf(scrollOffsetY / maxScrollOffset, 0.0f, 1.0f);
    return SDL_FRect{
        layout.graphicsScrollTrack.x,
        layout.graphicsScrollTrack.y + (thumbTravel * scrollRatio),
        layout.graphicsScrollTrack.w,
        thumbHeight
    };
}

static std::string getDisplayNameForScancode(SDL_Scancode scancode)
{
    if (scancode == SDL_SCANCODE_UNKNOWN)
    {
        return "Non attribué";
    }

    switch (scancode)
    {
        case SDL_SCANCODE_SPACE:
            return "Espace";
        case SDL_SCANCODE_TAB:
            return "Tab";
        case SDL_SCANCODE_RETURN:
            return "Entrée";
        case SDL_SCANCODE_KP_ENTER:
            return "Entrée num.";
        case SDL_SCANCODE_BACKSPACE:
            return "Retour";
        case SDL_SCANCODE_DELETE:
            return "Suppr.";
        case SDL_SCANCODE_UP:
            return "Flèche haut";
        case SDL_SCANCODE_DOWN:
            return "Flèche bas";
        case SDL_SCANCODE_LEFT:
            return "Flèche gauche";
        case SDL_SCANCODE_RIGHT:
            return "Flèche droite";
        case SDL_SCANCODE_LCTRL:
            return "Ctrl gauche";
        case SDL_SCANCODE_RCTRL:
            return "Ctrl droit";
        case SDL_SCANCODE_LSHIFT:
            return "Maj gauche";
        case SDL_SCANCODE_RSHIFT:
            return "Maj droit";
        case SDL_SCANCODE_LALT:
            return "Alt gauche";
        case SDL_SCANCODE_RALT:
            return "Alt droit";
        case SDL_SCANCODE_1:
            return "1";
        case SDL_SCANCODE_2:
            return "2";
        case SDL_SCANCODE_3:
            return "3";
        case SDL_SCANCODE_4:
            return "4";
        case SDL_SCANCODE_5:
            return "5";
        case SDL_SCANCODE_6:
            return "6";
        case SDL_SCANCODE_7:
            return "7";
        case SDL_SCANCODE_8:
            return "8";
        case SDL_SCANCODE_9:
            return "9";
        case SDL_SCANCODE_0:
            return "0";
        default:
            break;
    }

    const SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, true);
    const char* keyName = (keycode != SDLK_UNKNOWN) ? SDL_GetKeyName(keycode) : nullptr;
    if (keyName != nullptr && keyName[0] != '\0')
    {
        return std::string(keyName);
    }

    const char* scancodeName = SDL_GetScancodeName(scancode);
    return (scancodeName != nullptr && scancodeName[0] != '\0')
               ? std::string(scancodeName)
               : std::string("?");
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

static float measureTextWidth(RC2D_Font* font, const std::string& text)
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

static void drawTextAt(RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text.c_str());
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    rc2d_graphics_drawText(&textObject, std::round(x), std::round(y));
    rc2d_graphics_destroyText(&textObject);
}

static void drawCentered(RC2D_Font* font, const char* text, const SDL_FRect& rect, RC2D_Color color)
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

static void drawLeftCenteredY(RC2D_Font* font, const std::string& text, const SDL_FRect& rect, float x, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text textObject = rc2d_graphics_createText(font, text.c_str());
    textObject.color = color;
    rc2d_graphics_setTextColor(&textObject);
    int width = 0;
    int height = 0;
    rc2d_graphics_getTextSize(&textObject, &width, &height);
    (void)width;
    rc2d_graphics_drawText(
        &textObject,
        std::round(x),
        std::round(rect.y + ((rect.h - static_cast<float>(height)) * 0.5f)));
    rc2d_graphics_destroyText(&textObject);
}

static std::vector<std::string> buildWrappedLines(RC2D_Font* font, const std::string& text, float maxWidth)
{
    std::vector<std::string> lines;
    if (text.empty())
    {
        return lines;
    }

    std::stringstream stream(text);
    std::string word;
    std::string currentLine;
    while (stream >> word)
    {
        const std::string candidate = currentLine.empty() ? word : (currentLine + " " + word);
        if (currentLine.empty() || measureTextWidth(font, candidate) <= maxWidth)
        {
            currentLine = candidate;
            continue;
        }

        lines.push_back(currentLine);
        currentLine = word;
    }

    if (!currentLine.empty())
    {
        lines.push_back(currentLine);
    }

    if (lines.empty())
    {
        lines.push_back(text);
    }
    return lines;
}

static void drawWrappedText(
    RC2D_Font* font,
    const std::string& text,
    const SDL_FRect& rect,
    RC2D_Color color,
    float lineSpacing)
{
    const std::vector<std::string> lines = buildWrappedLines(font, text, rect.w);
    if (lines.empty())
    {
        return;
    }

    const float lineHeight = measureTextHeight(font, "Ag");
    float drawY = rect.y;
    for (const std::string& line : lines)
    {
        drawTextAt(font, line, rect.x, drawY, color);
        drawY += lineHeight + lineSpacing;
    }
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

static void drawCheckBox(const SDL_FRect& boxRect, bool checked, const RC2D_Image& checkedIcon)
{
    rc2d_graphics_setColor(kFieldFill);
    rc2d_graphics_rectangle("fill", &boxRect);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &boxRect);
    if (checked)
    {
        drawImageFit(checkedIcon, boxRect, 2.0f);
    }
}

static SDL_Rect toClipRect(const SDL_FRect& rect)
{
    return SDL_Rect{
        static_cast<int>(std::round(rect.x)),
        static_cast<int>(std::round(rect.y)),
        static_cast<int>(std::round(rect.w)),
        static_cast<int>(std::round(rect.h))
    };
}

#if 0
static char extractRedeemCharacter(const char* key)
{
    if (key == nullptr || key[0] == '\0')
    {
        return '\0';
    }

    if (key[1] == '\0')
    {
        unsigned char character = static_cast<unsigned char>(key[0]);
        if (std::isalnum(character) != 0 || key[0] == '-' || key[0] == '_')
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

    const bool looksLikeKeypad =
        (lower.find("kp") != std::string::npos) ||
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

#endif

static std::string sanitizeRedeemCodeValue(const std::string& rawValue)
{
    std::string filtered;
    filtered.reserve(rawValue.size());
    for (const char c : rawValue)
    {
        const unsigned char character = static_cast<unsigned char>(c);
        if (std::isalnum(character) == 0 && c != '-' && c != '_')
        {
            continue;
        }

        filtered.push_back(static_cast<char>(std::toupper(character)));
        if (filtered.size() >= kMaxRedeemCodeLength)
        {
            break;
        }
    }
    return filtered;
}

static int getLanguageMaxFirstRow(int totalRows, int visibleRows)
{
    return (std::max)(0, totalRows - visibleRows);
}

static int getVisibleLanguageRowCountForTotal(int totalRows)
{
    return (std::max)(1, (std::min)(kMaxVisibleLanguageRows, totalRows));
}

static GameSettingsLayout buildLayout(const SDL_FRect& outer, int languageOptionCount)
{
    GameSettingsLayout layout{};
    layout.outer = outer;
    layout.inner = SDL_FRect{outer.x + 4.0f, outer.y + 4.0f, outer.w - 8.0f, outer.h - 8.0f};
    layout.topBar = SDL_FRect{layout.inner.x + 1.0f, layout.inner.y + 1.0f, layout.inner.w - 2.0f, kTopBarHeight};
    layout.closeButton = SDL_FRect{
        outer.x + outer.w - 28.0f,
        layout.topBar.y + ((layout.topBar.h - 20.0f) * 0.5f),
        20.0f,
        20.0f
    };
    const float tabY = layout.topBar.y + 3.0f;
    const float tabH = layout.topBar.h - 6.0f;
    layout.tabs[0] = SDL_FRect{outer.x + 10.0f, tabY, 110.0f, tabH};
    layout.tabs[1] = SDL_FRect{layout.tabs[0].x + layout.tabs[0].w + 2.0f, tabY, 112.0f, tabH};
    layout.tabs[2] = SDL_FRect{layout.tabs[1].x + layout.tabs[1].w + 2.0f, tabY, 122.0f, tabH};
    layout.tabs[3] = SDL_FRect{layout.tabs[2].x + layout.tabs[2].w + 2.0f, tabY, 86.0f, tabH};

    const float dragLeft = layout.tabs[3].x + layout.tabs[3].w + 4.0f;
    const float dragRight = layout.closeButton.x - 4.0f;
    layout.headerDragRect =
        (dragRight > dragLeft)
            ? SDL_FRect{dragLeft, layout.topBar.y, dragRight - dragLeft, layout.topBar.h}
            : layout.topBar;

    const float contentY = layout.topBar.y + layout.topBar.h + 10.0f;
    layout.content = SDL_FRect{
        outer.x + 10.0f,
        contentY,
        outer.w - 20.0f,
        (outer.y + outer.h) - contentY - 10.0f
    };

    layout.redeemPanel = SDL_FRect{layout.content.x, layout.content.y, layout.content.w, 132.0f};
    layout.redeemButton = SDL_FRect{
        layout.redeemPanel.x + layout.redeemPanel.w - 154.0f,
        layout.redeemPanel.y + 80.0f,
        136.0f,
        34.0f
    };
    layout.redeemInput = SDL_FRect{
        layout.redeemPanel.x + 18.0f,
        layout.redeemButton.y,
        layout.redeemPanel.w - 18.0f - 12.0f - layout.redeemButton.w - 18.0f,
        34.0f
    };

    layout.configuratorPanel = SDL_FRect{
        layout.content.x,
        layout.redeemPanel.y + layout.redeemPanel.h + kSectionGap,
        layout.content.w,
        118.0f
    };
    layout.configuratorButton = SDL_FRect{
        layout.configuratorPanel.x + 18.0f,
        layout.configuratorPanel.y + 70.0f,
        272.0f,
        34.0f
    };

    layout.hudScalePanel = SDL_FRect{
        layout.content.x,
        layout.configuratorPanel.y + layout.configuratorPanel.h + kSectionGap,
        layout.content.w,
        kHudScalePanelHeight
    };

    const float hudScaleRowStartY = layout.hudScalePanel.y + kHudScaleRowsTopOffset;
    for (std::size_t index = 0; index < layout.hudScaleRows.size(); ++index)
    {
        layout.hudScaleRows[index] = SDL_FRect{
            layout.hudScalePanel.x + 18.0f,
            hudScaleRowStartY + (static_cast<float>(index) * (kHudScaleRowHeight + kHudScaleRowGap)),
            layout.hudScalePanel.w - 36.0f,
            kHudScaleRowHeight
        };
        layout.hudScaleVisibilityButtons[index] = SDL_FRect{
            layout.hudScaleRows[index].x + layout.hudScaleRows[index].w - kHudScaleToggleButtonWidth,
            layout.hudScaleRows[index].y + ((layout.hudScaleRows[index].h - kHudScaleToggleButtonHeight) * 0.5f),
            kHudScaleToggleButtonWidth,
            kHudScaleToggleButtonHeight
        };
        layout.hudScaleTracks[index] = SDL_FRect{
            layout.hudScaleRows[index].x + kHudScaleTrackLeftOffset,
            layout.hudScaleRows[index].y + ((layout.hudScaleRows[index].h - kHudScaleTrackHeight) * 0.5f),
            (std::max)(
                1.0f,
                layout.hudScaleVisibilityButtons[index].x - 12.0f - kHudScaleValueLabelWidth - 12.0f - (layout.hudScaleRows[index].x + kHudScaleTrackLeftOffset)),
            kHudScaleTrackHeight
        };
    }

    layout.mapFramePanel = SDL_FRect{
        layout.content.x,
        layout.hudScalePanel.y + layout.hudScalePanel.h + kSectionGap,
        layout.content.w,
        kMapFramePanelHeight
    };
    const float mapFrameRowStartY = layout.mapFramePanel.y + kMapFrameRowsTopOffset;
    for (std::size_t mapFrameIndex = 0; mapFrameIndex < layout.mapFrameRows.size(); ++mapFrameIndex)
    {
        layout.mapFrameRows[mapFrameIndex] = SDL_FRect{
            layout.mapFramePanel.x + 18.0f,
            mapFrameRowStartY + (static_cast<float>(mapFrameIndex) * (kMapFrameRowHeight + kMapFrameRowGap)),
            layout.mapFramePanel.w - 36.0f,
            kMapFrameRowHeight
        };
        const float valueRight = layout.mapFrameRows[mapFrameIndex].x + layout.mapFrameRows[mapFrameIndex].w - kHudScaleValueLabelWidth;
        layout.mapFrameTracks[mapFrameIndex] = SDL_FRect{
            layout.mapFrameRows[mapFrameIndex].x + kHudScaleTrackLeftOffset,
            layout.mapFrameRows[mapFrameIndex].y + ((layout.mapFrameRows[mapFrameIndex].h - kHudScaleTrackHeight) * 0.5f),
            (std::max)(
                1.0f,
                valueRight - 12.0f - (layout.mapFrameRows[mapFrameIndex].x + kHudScaleTrackLeftOffset)),
            kHudScaleTrackHeight
        };
    }

    layout.languageRow = SDL_FRect{
        layout.content.x,
        layout.mapFramePanel.y + layout.mapFramePanel.h + kSectionGap,
        layout.content.w,
        (layout.content.y + layout.content.h) - (layout.mapFramePanel.y + layout.mapFramePanel.h + kSectionGap)
    };
    layout.languageButton = SDL_FRect{
        layout.languageRow.x + layout.languageRow.w - 340.0f,
        layout.languageRow.y + ((layout.languageRow.h - 34.0f) * 0.5f),
        322.0f,
        34.0f
    };

    const int visibleLanguageRows = getVisibleLanguageRowCountForTotal(languageOptionCount);
    const float dropdownHeight = 8.0f + (static_cast<float>(visibleLanguageRows) * kLanguageRowHeight) + 8.0f;
    const float languageDropdownY = (std::max)(layout.content.y, layout.languageButton.y - 4.0f - dropdownHeight);
    layout.languageDropdown = SDL_FRect{
        layout.languageButton.x,
        languageDropdownY,
        layout.languageButton.w,
        dropdownHeight
    };

    const bool showLanguageScrollBar = languageOptionCount > visibleLanguageRows;
    const float scrollReserve = showLanguageScrollBar ? (kScrollBarWidth + (kScrollBarPadding * 2.0f)) : 0.0f;
    layout.languageViewport = SDL_FRect{
        layout.languageDropdown.x + 4.0f,
        layout.languageDropdown.y + 4.0f,
        layout.languageDropdown.w - 8.0f - scrollReserve,
        layout.languageDropdown.h - 8.0f
    };
    layout.languageScrollTrack = SDL_FRect{
        layout.languageDropdown.x + layout.languageDropdown.w - (kScrollBarWidth + kScrollBarPadding),
        layout.languageDropdown.y + kScrollBarPadding,
        kScrollBarWidth,
        layout.languageDropdown.h - (kScrollBarPadding * 2.0f)
    };

    layout.placeholderPanel = SDL_FRect{
        layout.content.x,
        layout.content.y,
        layout.content.w,
        layout.content.h
    };
    layout.controlsPanel = layout.placeholderPanel;
    layout.controlsResetAllButton = SDL_FRect{
        layout.controlsPanel.x + layout.controlsPanel.w - kControlsResetButtonWidth - 14.0f,
        layout.controlsPanel.y + layout.controlsPanel.h - kControlsFooterHeight + 9.0f,
        kControlsResetButtonWidth,
        kControlsButtonHeight
    };
    layout.controlsConflictPrompt = SDL_FRect{
        layout.controlsPanel.x + 14.0f,
        layout.controlsResetAllButton.y,
        (std::max)(1.0f, layout.controlsResetAllButton.x - layout.controlsPanel.x - 28.0f),
        kControlsButtonHeight
    };
    layout.controlsConflictCancelButton = SDL_FRect{
        layout.controlsConflictPrompt.x + layout.controlsConflictPrompt.w - 86.0f,
        layout.controlsConflictPrompt.y,
        86.0f,
        kControlsButtonHeight
    };
    layout.controlsConflictConfirmButton = SDL_FRect{
        layout.controlsConflictCancelButton.x - 96.0f,
        layout.controlsConflictPrompt.y,
        90.0f,
        kControlsButtonHeight
    };
    layout.controlsScrollTrack = SDL_FRect{
        layout.controlsPanel.x + layout.controlsPanel.w - kScrollBarWidth - kScrollBarPadding,
        layout.controlsPanel.y + kControlsViewportPadding,
        kScrollBarWidth,
        layout.controlsPanel.h - kControlsFooterHeight - (kControlsViewportPadding * 2.0f)
    };
    layout.controlsViewport = SDL_FRect{
        layout.controlsPanel.x + kControlsViewportPadding,
        layout.controlsPanel.y + kControlsViewportPadding,
        layout.controlsPanel.w - (kControlsViewportPadding * 2.0f) - kScrollBarWidth - (kScrollBarPadding * 2.0f),
        layout.controlsPanel.h - kControlsFooterHeight - (kControlsViewportPadding * 2.0f)
    };

    layout.graphicsPanel = layout.placeholderPanel;
    layout.graphicsScrollTrack = SDL_FRect{
        layout.graphicsPanel.x + layout.graphicsPanel.w - kScrollBarWidth - kScrollBarPadding,
        layout.graphicsPanel.y + kGraphicsViewportPadding,
        kScrollBarWidth,
        layout.graphicsPanel.h - (kGraphicsViewportPadding * 2.0f)
    };
    layout.graphicsViewport = SDL_FRect{
        layout.graphicsPanel.x + kGraphicsViewportPadding,
        layout.graphicsPanel.y + kGraphicsViewportPadding,
        layout.graphicsPanel.w - (kGraphicsViewportPadding * 2.0f) - kScrollBarWidth - (kScrollBarPadding * 2.0f),
        layout.graphicsPanel.h - (kGraphicsViewportPadding * 2.0f)
    };
    layout.graphicsGeneralPanel = SDL_FRect{
        layout.graphicsViewport.x,
        layout.graphicsViewport.y,
        layout.graphicsViewport.w,
        kGraphicsSectionGeneralHeight
    };
    layout.graphicsVsyncRow = SDL_FRect{
        layout.graphicsGeneralPanel.x + 16.0f,
        layout.graphicsGeneralPanel.y + 52.0f,
        layout.graphicsGeneralPanel.w - 32.0f,
        42.0f
    };
    layout.graphicsVsyncCheckbox = SDL_FRect{
        layout.graphicsVsyncRow.x + 300.0f,
        layout.graphicsVsyncRow.y + ((layout.graphicsVsyncRow.h - kGraphicsCheckboxSize) * 0.5f),
        kGraphicsCheckboxSize,
        kGraphicsCheckboxSize
    };
    layout.graphicsWindowPanel = SDL_FRect{
        layout.graphicsViewport.x,
        layout.graphicsGeneralPanel.y + layout.graphicsGeneralPanel.h + kSectionGap,
        layout.graphicsViewport.w,
        kGraphicsSectionWindowHeight
    };
    layout.graphicsWindowModeRow = SDL_FRect{
        layout.graphicsWindowPanel.x + 16.0f,
        layout.graphicsWindowPanel.y + 52.0f,
        layout.graphicsWindowPanel.w - 32.0f,
        kGraphicsSelectHeight
    };
    layout.graphicsWindowModeButton = SDL_FRect{
        layout.graphicsWindowModeRow.x + layout.graphicsWindowModeRow.w - kGraphicsSelectWidth,
        layout.graphicsWindowModeRow.y,
        kGraphicsSelectWidth,
        kGraphicsSelectHeight
    };
    layout.graphicsWindowModeDropdown = SDL_FRect{
        layout.graphicsWindowModeButton.x,
        layout.graphicsWindowModeButton.y + layout.graphicsWindowModeButton.h + 4.0f,
        layout.graphicsWindowModeButton.w,
        8.0f + (3.0f * kGraphicsOptionRowHeight)
    };
    for (std::size_t index = 0; index < layout.graphicsWindowModeOptions.size(); ++index)
    {
        layout.graphicsWindowModeOptions[index] = SDL_FRect{
            layout.graphicsWindowModeDropdown.x + 4.0f,
            layout.graphicsWindowModeDropdown.y + 4.0f + (static_cast<float>(index) * kGraphicsOptionRowHeight),
            layout.graphicsWindowModeDropdown.w - 8.0f,
            kGraphicsOptionRowHeight
        };
    }
    layout.graphicsPresentationModeRow = SDL_FRect{
        layout.graphicsWindowPanel.x + 16.0f,
        layout.graphicsWindowModeRow.y + layout.graphicsWindowModeRow.h + 14.0f,
        layout.graphicsWindowPanel.w - 32.0f,
        kGraphicsSelectHeight
    };
    layout.graphicsPresentationModeButton = SDL_FRect{
        layout.graphicsPresentationModeRow.x + layout.graphicsPresentationModeRow.w - kGraphicsSelectWidth,
        layout.graphicsPresentationModeRow.y,
        kGraphicsSelectWidth,
        kGraphicsSelectHeight
    };
    layout.graphicsPresentationModeDropdown = SDL_FRect{
        layout.graphicsPresentationModeButton.x,
        layout.graphicsPresentationModeButton.y + layout.graphicsPresentationModeButton.h + 4.0f,
        layout.graphicsPresentationModeButton.w,
        8.0f + (2.0f * kGraphicsOptionRowHeight)
    };
    for (std::size_t index = 0; index < layout.graphicsPresentationModeOptions.size(); ++index)
    {
        layout.graphicsPresentationModeOptions[index] = SDL_FRect{
            layout.graphicsPresentationModeDropdown.x + 4.0f,
            layout.graphicsPresentationModeDropdown.y + 4.0f + (static_cast<float>(index) * kGraphicsOptionRowHeight),
            layout.graphicsPresentationModeDropdown.w - 8.0f,
            kGraphicsOptionRowHeight
        };
    }
    layout.graphicsMonitorFpsRow = SDL_FRect{
        layout.graphicsWindowPanel.x + 16.0f,
        layout.graphicsPresentationModeRow.y + layout.graphicsPresentationModeRow.h + 18.0f,
        layout.graphicsWindowPanel.w - 32.0f,
        kGraphicsSelectHeight
    };
    layout.graphicsMonitorFpsBox = SDL_FRect{
        layout.graphicsMonitorFpsRow.x + layout.graphicsMonitorFpsRow.w - kGraphicsSelectWidth,
        layout.graphicsMonitorFpsRow.y,
        kGraphicsSelectWidth,
        layout.graphicsMonitorFpsRow.h
    };
    layout.graphicsCursorLockRow = SDL_FRect{
        layout.graphicsWindowPanel.x + 16.0f,
        layout.graphicsMonitorFpsRow.y + layout.graphicsMonitorFpsRow.h + 12.0f,
        layout.graphicsWindowPanel.w - 32.0f,
        42.0f
    };
    layout.graphicsCursorLockCheckbox = SDL_FRect{
        layout.graphicsCursorLockRow.x + 300.0f,
        layout.graphicsCursorLockRow.y + ((layout.graphicsCursorLockRow.h - kGraphicsCheckboxSize) * 0.5f),
        kGraphicsCheckboxSize,
        kGraphicsCheckboxSize
    };
    layout.graphicsInterfacePanel = SDL_FRect{
        layout.graphicsViewport.x,
        layout.graphicsWindowPanel.y + layout.graphicsWindowPanel.h + kSectionGap,
        layout.graphicsViewport.w,
        kGraphicsSectionInterfaceHeight
    };
    layout.graphicsHideCoordinateBackgroundRow = SDL_FRect{
        layout.graphicsInterfacePanel.x + 16.0f,
        layout.graphicsInterfacePanel.y + 52.0f,
        layout.graphicsInterfacePanel.w - 32.0f,
        42.0f
    };
    layout.graphicsHideCoordinateBackgroundCheckbox = SDL_FRect{
        layout.graphicsHideCoordinateBackgroundRow.x + 300.0f,
        layout.graphicsHideCoordinateBackgroundRow.y + ((layout.graphicsHideCoordinateBackgroundRow.h - kGraphicsCheckboxSize) * 0.5f),
        kGraphicsCheckboxSize,
        kGraphicsCheckboxSize
    };
    layout.graphicsAnimationsPanel = SDL_FRect{
        layout.graphicsViewport.x,
        layout.graphicsInterfacePanel.y + layout.graphicsInterfacePanel.h + kSectionGap,
        layout.graphicsViewport.w,
        kGraphicsSectionAnimationsHeight
    };
    layout.graphicsFogOfWarRow = SDL_FRect{
        layout.graphicsAnimationsPanel.x + 16.0f,
        layout.graphicsAnimationsPanel.y + 52.0f,
        layout.graphicsAnimationsPanel.w - 32.0f,
        kGraphicsOptionRowHeight
    };
    layout.graphicsFogOfWarCheckbox = SDL_FRect{
        (layout.graphicsFogOfWarRow.x + layout.graphicsFogOfWarRow.w) - kGraphicsCheckboxSize,
        layout.graphicsFogOfWarRow.y + ((layout.graphicsFogOfWarRow.h - kGraphicsCheckboxSize) * 0.5f),
        kGraphicsCheckboxSize,
        kGraphicsCheckboxSize
    };
    layout.graphicsShipWakeTrailsRow = SDL_FRect{
        layout.graphicsAnimationsPanel.x + 16.0f,
        layout.graphicsFogOfWarRow.y + layout.graphicsFogOfWarRow.h + 4.0f,
        layout.graphicsAnimationsPanel.w - 32.0f,
        kGraphicsOptionRowHeight
    };
    layout.graphicsShipWakeTrailsCheckbox = SDL_FRect{
        (layout.graphicsShipWakeTrailsRow.x + layout.graphicsShipWakeTrailsRow.w) - kGraphicsCheckboxSize,
        layout.graphicsShipWakeTrailsRow.y + ((layout.graphicsShipWakeTrailsRow.h - kGraphicsCheckboxSize) * 0.5f),
        kGraphicsCheckboxSize,
        kGraphicsCheckboxSize
    };
    layout.graphicsHideOtherPlayersVfxRow = SDL_FRect{
        layout.graphicsAnimationsPanel.x + 16.0f,
        layout.graphicsShipWakeTrailsRow.y + layout.graphicsShipWakeTrailsRow.h + 4.0f,
        layout.graphicsAnimationsPanel.w - 32.0f,
        kGraphicsOptionRowHeight
    };
    layout.graphicsHideOtherPlayersVfxCheckbox = SDL_FRect{
        (layout.graphicsHideOtherPlayersVfxRow.x + layout.graphicsHideOtherPlayersVfxRow.w) - kGraphicsCheckboxSize,
        layout.graphicsHideOtherPlayersVfxRow.y + ((layout.graphicsHideOtherPlayersVfxRow.h - kGraphicsCheckboxSize) * 0.5f),
        kGraphicsCheckboxSize,
        kGraphicsCheckboxSize
    };

    layout.graphicsGameplayPanel = SDL_FRect{
        layout.graphicsViewport.x,
        layout.graphicsAnimationsPanel.y + layout.graphicsAnimationsPanel.h + kSectionGap,
        layout.graphicsViewport.w,
        kGraphicsSectionGameplayHeight
    };
    layout.graphicsSalvoRow = SDL_FRect{
        layout.graphicsGameplayPanel.x + 16.0f,
        layout.graphicsGameplayPanel.y + 52.0f,
        layout.graphicsGameplayPanel.w - 32.0f,
        kGraphicsSelectHeight
    };
    const float salvoOptionsX = layout.graphicsSalvoRow.x + layout.graphicsSalvoRow.w - kGraphicsSelectWidth;
    const float optionW = std::floor(kGraphicsSelectWidth / 3.0f);
    for (std::size_t index = 0; index < layout.graphicsSalvoOptions.size(); ++index)
    {
        const float x = salvoOptionsX + (static_cast<float>(index) * optionW);
        const float w =
            (index == layout.graphicsSalvoOptions.size() - 1U)
                ? (salvoOptionsX + kGraphicsSelectWidth - x)
                : optionW;
        layout.graphicsSalvoOptions[index] = SDL_FRect{
            x,
            layout.graphicsSalvoRow.y,
            w,
            layout.graphicsSalvoRow.h
        };
    }

    return layout;
}

GameSettingsWidget::GameSettingsWidget(void)
    : titleFont{},
      bodyFont{},
      smallFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      cursorEnabled(true),
      resourcesLoaded(false),
      controlIcons{},
      arrowDownIcon{},
      checkboxValidIcon{},
      activeTab(SettingsTab::GENERAL),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      languageOptions{},
      selectedLanguageIndex(-1),
      hoveredLanguageIndex(-1),
      languageFirstRow(0),
      languageDropdownOpen(false),
      languageScrollDragging(false),
      languageScrollDragOffsetY(0.0f),
      languageScrollWheelHighlightSec(0.0f),
      redeemCode{},
      redeemInputFocused(false),
      redeemCursorIndex(0U),
      redeemSelectionAnchorIndex(0U),
      redeemInputSelectingWithMouse(false),
      redeemCursorVisible(false),
      redeemCursorBlinkElapsed(0.0),
      hudScaleValues{},
      hudVisibilityValues{},
      hudScaleDragging(false),
      draggedHudScaleTarget(HudScaleTarget::MINIMAP),
      hudScaleDragGrabOffsetX(0.0f),
      controlActionScancodes{},
      cameraScrollSpeedSectors(kCameraScrollSpeedDefaultSectors),
      controlsScrollOffsetY(0.0f),
      controlsScrollDragging(false),
      controlsScrollDragOffsetY(0.0f),
      controlsScrollWheelHighlightSec(0.0f),
      cameraScrollSpeedDragging(false),
      cameraScrollSpeedDragGrabOffsetX(0.0f),
      controlCaptureActive(false),
      capturedControlAction(ControlAction::CAMERA_MOVE_UP),
      controlConflictPending(false),
      pendingConflictAction(ControlAction::CAMERA_MOVE_UP),
      conflictingControlAction(ControlAction::CAMERA_MOVE_UP),
      pendingConflictScancode(SDL_SCANCODE_UNKNOWN),
      graphicsWindowModeDropdownOpen(false),
      graphicsPresentationModeDropdownOpen(false),
      graphicsHideCoordinateBackground(false),
      graphicsHideOtherPlayersVfx(false),
      graphicsScrollOffsetY(0.0f),
      graphicsScrollDragging(false),
      graphicsScrollDragOffsetY(0.0f),
      graphicsScrollWheelHighlightSec(0.0f),
      graphicsSalvoBulletPreset(SalvoBulletPreset::NORMAL),
      onRedeemCodeRequested{},
      onStartUiConfiguratorRequested{},
      onLanguageChanged{},
      onHudScaleChanged{},
      onHudVisibilityChanged{},
      onUserSettingsChanged{},
      mapPlayfieldFrameMarginPercent{0, 0, 0},
      mapFrameMarginDragging(false),
      mapFrameMarginDragRow(0U),
      mapFrameMarginDragGrabOffsetX(0.0f)
{
    this->hudScaleValues.fill(1.0f);
    this->hudVisibilityValues.fill(true);
    this->resetAllControlActionScancodes();
    this->applyMapPlayfieldFrameMargins(false);
}

GameSettingsWidget::~GameSettingsWidget(void)
{
}

void GameSettingsWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->smallFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 13.0f);
    this->arrowDownIcon = LoadStorageImage("assets/images/ui-scene-game/icon-arrowdown.png", RC2D_STORAGE_TITLE);
    this->checkboxValidIcon = LoadStorageImage("assets/images/ui-scene-game/icon-checkboxvalid.png", RC2D_STORAGE_TITLE);
    this->controlIcons.load();

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};
    this->visible = false;
    this->activeTab = SettingsTab::GENERAL;
    this->widgetDragging = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->redeemInputFocused = false;
    this->redeemCursorIndex = this->redeemCode.size();
    this->redeemSelectionAnchorIndex = this->redeemCursorIndex;
    this->redeemEditHistory.clear();
    this->redeemInputSelectingWithMouse = false;
    this->redeemCursorVisible = false;
    this->redeemCursorBlinkElapsed = 0.0;
    this->hudScaleDragging = false;
    this->mapFrameMarginDragging = false;
    this->draggedHudScaleTarget = HudScaleTarget::MINIMAP;
    this->hudScaleDragGrabOffsetX = 0.0f;
    this->controlsScrollOffsetY = 0.0f;
    this->controlsScrollDragging = false;
    this->controlsScrollDragOffsetY = 0.0f;
    this->controlsScrollWheelHighlightSec = 0.0f;
    this->cameraScrollSpeedDragging = false;
    this->cameraScrollSpeedDragGrabOffsetX = 0.0f;
    this->controlCaptureActive = false;
    this->capturedControlAction = ControlAction::CAMERA_MOVE_UP;
    this->controlConflictPending = false;
    this->pendingConflictAction = ControlAction::CAMERA_MOVE_UP;
    this->conflictingControlAction = ControlAction::CAMERA_MOVE_UP;
    this->pendingConflictScancode = SDL_SCANCODE_UNKNOWN;
    this->graphicsWindowModeDropdownOpen = false;
    this->graphicsPresentationModeDropdownOpen = false;
    this->graphicsHideCoordinateBackground = false;
    this->graphicsHideOtherPlayersVfx = false;
    this->graphicsScrollOffsetY = 0.0f;
    this->graphicsScrollDragging = false;
    this->graphicsScrollDragOffsetY = 0.0f;
    this->graphicsScrollWheelHighlightSec = 0.0f;
    this->languageScrollWheelHighlightSec = 0.0f;
    this->graphicsSalvoBulletPreset = SalvoBulletPreset::NORMAL;
    g_graphicsFogOfWarEnabled = true;
    g_graphicsShipWakeTrailsEnabled = true;
    this->cameraScrollSpeedSectors = snapCameraScrollSpeed(this->cameraScrollSpeedSectors);
    this->hudVisibilityValues.fill(true);
    this->resourcesLoaded = true;

    this->rebuildLanguageOptions();
    this->syncSelectedLanguageFromContext();
}

void GameSettingsWidget::unload(void)
{
    this->resourcesLoaded = false;
    this->languageOptions.clear();
    this->controlsScrollDragging = false;
    this->cameraScrollSpeedDragging = false;
    this->controlCaptureActive = false;
    this->controlConflictPending = false;
    this->graphicsWindowModeDropdownOpen = false;
    this->graphicsPresentationModeDropdownOpen = false;
    this->controlIcons.unload();
    this->redeemEditHistory.clear();
    ResetStorageImageRef(&this->checkboxValidIcon);
    ResetStorageImageRef(&this->arrowDownIcon);
    ResetStorageFontRef(&this->smallFont);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

void GameSettingsWidget::update(double dt)
{
    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (!this->visible)
    {
        this->languageScrollWheelHighlightSec = 0.0f;
        this->controlsScrollWheelHighlightSec = 0.0f;
        this->graphicsScrollWheelHighlightSec = 0.0f;
        return;
    }

    const float wheelDt = static_cast<float>(dt);
    if (this->languageScrollWheelHighlightSec > 0.0f)
    {
        this->languageScrollWheelHighlightSec -= wheelDt;
        if (this->languageScrollWheelHighlightSec < 0.0f)
        {
            this->languageScrollWheelHighlightSec = 0.0f;
        }
    }
    if (this->controlsScrollWheelHighlightSec > 0.0f)
    {
        this->controlsScrollWheelHighlightSec -= wheelDt;
        if (this->controlsScrollWheelHighlightSec < 0.0f)
        {
            this->controlsScrollWheelHighlightSec = 0.0f;
        }
    }
    if (this->graphicsScrollWheelHighlightSec > 0.0f)
    {
        this->graphicsScrollWheelHighlightSec -= wheelDt;
        if (this->graphicsScrollWheelHighlightSec < 0.0f)
        {
            this->graphicsScrollWheelHighlightSec = 0.0f;
        }
    }

    this->syncSelectedLanguageFromContext();
    this->clampLanguageScroll();
    this->clampControlsScroll();

    if (this->redeemInputSelectingWithMouse)
    {
        if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
        {
            this->redeemInputSelectingWithMouse = false;
        }
        else if (this->redeemInputFocused)
        {
            const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            getMouseRenderPosition(&mouseX, &mouseY);
            (void)mouseY;
            this->redeemCursorIndex = this->getRedeemCursorIndexFromPosition(mouseX, layout.redeemInput);
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
        }
    }

    if (this->redeemInputFocused)
    {
        this->redeemCursorBlinkElapsed += dt;
        if (this->redeemCursorBlinkElapsed >= kCursorBlinkPeriod)
        {
            this->redeemCursorBlinkElapsed = 0.0;
            this->redeemCursorVisible = !this->redeemCursorVisible;
        }
    }
    else
    {
        this->redeemCursorVisible = false;
        this->redeemCursorBlinkElapsed = 0.0;
    }

    this->hoveredLanguageIndex = -1;
    if (this->languageDropdownOpen)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);

        const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
        const int visibleRows = this->getVisibleLanguageRowCount();
        if (isPointInRect(mouseX, mouseY, layout.languageViewport))
        {
            for (int row = 0; row < visibleRows; ++row)
            {
                const int optionIndex = this->languageFirstRow + row;
                if (optionIndex < 0 || optionIndex >= static_cast<int>(this->languageOptions.size()))
                {
                    break;
                }

                const SDL_FRect rowRect = SDL_FRect{
                    layout.languageViewport.x,
                    layout.languageViewport.y + (static_cast<float>(row) * kLanguageRowHeight),
                    layout.languageViewport.w,
                    kLanguageRowHeight
                };
                if (isPointInRect(mouseX, mouseY, rowRect))
                {
                    this->hoveredLanguageIndex = optionIndex;
                    break;
                }
            }
        }
    }

    if (!this->widgetDragging &&
        !this->languageScrollDragging &&
        !this->hudScaleDragging &&
        !this->mapFrameMarginDragging &&
        !this->controlsScrollDragging &&
        !this->cameraScrollSpeedDragging &&
        !this->graphicsScrollDragging)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        const bool endedCameraSpeedDrag = this->cameraScrollSpeedDragging;
        const bool endedMapFrameMarginDrag = this->mapFrameMarginDragging;
        this->widgetDragging = false;
        this->languageScrollDragging = false;
        this->hudScaleDragging = false;
        this->mapFrameMarginDragging = false;
        this->controlsScrollDragging = false;
        this->cameraScrollSpeedDragging = false;
        this->graphicsScrollDragging = false;
        if (endedCameraSpeedDrag && this->onUserSettingsChanged)
        {
            this->onUserSettingsChanged();
        }
        if (endedMapFrameMarginDrag && this->onUserSettingsChanged)
        {
            this->onUserSettingsChanged();
        }
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);

    if (this->hudScaleDragging)
    {
        this->updateDraggedHudScaleFromMouse(mouseX);
        return;
    }

    if (this->mapFrameMarginDragging)
    {
        this->updateDraggedMapFrameMarginFromMouse(mouseX);
        return;
    }

    if (this->cameraScrollSpeedDragging)
    {
        this->updateDraggedCameraScrollSpeedFromMouse(mouseX);
        return;
    }

    if (this->controlsScrollDragging)
    {
        this->updateControlsScrollFromMouse(mouseY);
        return;
    }

    if (this->graphicsScrollDragging)
    {
        this->updateGraphicsScrollFromMouse(mouseY);
        return;
    }

    if (this->languageScrollDragging)
    {
        const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
        const int visibleRows = this->getVisibleLanguageRowCount();
        const int maxFirstRow = getLanguageMaxFirstRow(static_cast<int>(this->languageOptions.size()), visibleRows);
        if (maxFirstRow <= 0)
        {
            this->languageFirstRow = 0;
            this->languageScrollDragging = false;
            return;
        }

        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            layout.languageScrollTrack.h *
                (static_cast<float>(visibleRows) / static_cast<float>((std::max)(1, static_cast<int>(this->languageOptions.size())))));
        const float thumbTravel = (std::max)(1.0f, layout.languageScrollTrack.h - thumbHeight);
        const float thumbTop = clampf(
            mouseY - this->languageScrollDragOffsetY,
            layout.languageScrollTrack.y,
            layout.languageScrollTrack.y + thumbTravel);
        const float scrollRatio = (thumbTop - layout.languageScrollTrack.y) / thumbTravel;
        this->languageFirstRow = static_cast<int>(scrollRatio * static_cast<float>(maxFirstRow) + 0.5f);
        this->clampLanguageScroll();
        return;
    }

    this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
    this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
    this->widgetRect.x = baseRect.x + this->widgetOffsetX;
    this->widgetRect.y = baseRect.y + this->widgetOffsetY;
}

bool GameSettingsWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)mouseID;
    if (!this->visible || button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return false;
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    if (!isPointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    if (isPointInRect(x, y, layout.closeButton))
    {
        this->hide();
        return true;
    }

    for (int tabIndex = 0; tabIndex < static_cast<int>(layout.tabs.size()); ++tabIndex)
    {
        if (!isPointInRect(x, y, layout.tabs[static_cast<std::size_t>(tabIndex)]))
        {
            continue;
        }

        this->activeTab = static_cast<SettingsTab>(tabIndex);
        this->clearFocus();
        return true;
    }

    if (this->activeTab == SettingsTab::GENERAL)
    {
        if (this->languageDropdownOpen)
        {
            if (isPointInRect(x, y, layout.languageScrollTrack) &&
                getLanguageMaxFirstRow(static_cast<int>(this->languageOptions.size()), this->getVisibleLanguageRowCount()) > 0)
            {
                const int visibleRows = this->getVisibleLanguageRowCount();
                const float thumbHeight = (std::max)(
                    kMinThumbHeight,
                    layout.languageScrollTrack.h *
                        (static_cast<float>(visibleRows) /
                         static_cast<float>((std::max)(1, static_cast<int>(this->languageOptions.size())))));
                const float thumbTravel = (std::max)(1.0f, layout.languageScrollTrack.h - thumbHeight);
                const int maxFirstRow = getLanguageMaxFirstRow(static_cast<int>(this->languageOptions.size()), visibleRows);
                const float currentRatio =
                    static_cast<float>((std::max)(0, (std::min)(this->languageFirstRow, maxFirstRow))) /
                    static_cast<float>(maxFirstRow);
                const SDL_FRect thumbRect = SDL_FRect{
                    layout.languageScrollTrack.x,
                    layout.languageScrollTrack.y + (thumbTravel * currentRatio),
                    layout.languageScrollTrack.w,
                    thumbHeight
                };

                this->languageScrollDragging = true;
                this->hudScaleDragging = false;
                this->mapFrameMarginDragging = false;
                this->widgetDragging = false;
                if (isPointInRect(x, y, thumbRect))
                {
                    this->languageScrollDragOffsetY = y - thumbRect.y;
                }
                else
                {
                    this->languageScrollDragOffsetY = thumbHeight * 0.5f;
                }
                return true;
            }

            if (isPointInRect(x, y, layout.languageViewport))
            {
                const int visibleRows = this->getVisibleLanguageRowCount();
                for (int row = 0; row < visibleRows; ++row)
                {
                    const int optionIndex = this->languageFirstRow + row;
                    if (optionIndex < 0 || optionIndex >= static_cast<int>(this->languageOptions.size()))
                    {
                        break;
                    }

                    const SDL_FRect rowRect = SDL_FRect{
                        layout.languageViewport.x,
                        layout.languageViewport.y + (static_cast<float>(row) * kLanguageRowHeight),
                        layout.languageViewport.w,
                        kLanguageRowHeight
                    };
                    if (!isPointInRect(x, y, rowRect))
                    {
                        continue;
                    }

                    this->selectedLanguageIndex = optionIndex;
                    this->applySelectedLanguageToContext();
                    this->languageDropdownOpen = false;
                    this->languageScrollDragging = false;
                    this->hoveredLanguageIndex = -1;
                    return true;
                }
            }

            if (isPointInRect(x, y, layout.languageDropdown))
            {
                return true;
            }

            if (!isPointInRect(x, y, layout.languageButton))
            {
                this->languageDropdownOpen = false;
                this->languageScrollDragging = false;
                this->hoveredLanguageIndex = -1;
            }
        }

        if (isPointInRect(x, y, layout.redeemButton))
        {
            this->hudScaleDragging = false;
            this->mapFrameMarginDragging = false;
            this->triggerRedeemCodeRequest();
            return true;
        }

        if (isPointInRect(x, y, layout.redeemInput))
        {
            this->redeemInputFocused = true;
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            this->hudScaleDragging = false;
            this->mapFrameMarginDragging = false;
            if (clicks >= 2)
            {
                this->redeemSelectionAnchorIndex = 0U;
                this->redeemCursorIndex = this->redeemCode.size();
                this->redeemInputSelectingWithMouse = false;
                return true;
            }

            const std::size_t clickedIndex = this->getRedeemCursorIndexFromPosition(x, layout.redeemInput);
            this->redeemCursorIndex = clickedIndex;
            this->redeemSelectionAnchorIndex = clickedIndex;
            this->redeemInputSelectingWithMouse = true;
            return true;
        }

        if (isPointInRect(x, y, layout.configuratorButton))
        {
            this->hudScaleDragging = false;
            this->mapFrameMarginDragging = false;
            this->triggerUiConfiguratorRequest();
            return true;
        }

        for (std::size_t index = 0; index < layout.hudScaleVisibilityButtons.size(); ++index)
        {
            if (!isPointInRect(x, y, layout.hudScaleVisibilityButtons[index]))
            {
                continue;
            }

            this->redeemInputFocused = false;
            this->redeemInputSelectingWithMouse = false;
            this->clearRedeemSelection();
            this->languageDropdownOpen = false;
            this->languageScrollDragging = false;
            this->widgetDragging = false;
            this->hudScaleDragging = false;
            this->mapFrameMarginDragging = false;
            this->applyHudVisibilityValue(
                static_cast<HudScaleTarget>(index),
                !this->hudVisibilityValues[index],
                true);
            return true;
        }

        for (std::size_t index = 0; index < layout.hudScaleTracks.size(); ++index)
        {
            const SDL_FRect& trackRect = layout.hudScaleTracks[index];
            const HudScaleTarget target = static_cast<HudScaleTarget>(index);
            const SDL_FRect thumbRect = getHudScaleThumbRect(trackRect, target, this->hudScaleValues[index]);
            if (!isPointInRect(x, y, trackRect) && !isPointInRect(x, y, thumbRect))
            {
                continue;
            }

            this->redeemInputFocused = false;
            this->redeemInputSelectingWithMouse = false;
            this->clearRedeemSelection();
            this->languageDropdownOpen = false;
            this->languageScrollDragging = false;
            this->widgetDragging = false;
            this->hudScaleDragging = true;
            this->mapFrameMarginDragging = false;
            this->draggedHudScaleTarget = target;
            this->hudScaleDragGrabOffsetX = isPointInRect(x, y, thumbRect)
                                                ? (x - thumbRect.x)
                                                : (kHudScaleThumbWidth * 0.5f);
            this->updateDraggedHudScaleFromMouse(x);
            return true;
        }

        for (std::size_t index = 0; index < layout.mapFrameTracks.size(); ++index)
        {
            const SDL_FRect& trackRect = layout.mapFrameTracks[index];
            const SDL_FRect thumbRect =
                getMapFrameMarginThumbRect(trackRect, this->mapPlayfieldFrameMarginPercent[index]);
            if (!isPointInRect(x, y, trackRect) && !isPointInRect(x, y, thumbRect))
            {
                continue;
            }

            this->redeemInputFocused = false;
            this->redeemInputSelectingWithMouse = false;
            this->clearRedeemSelection();
            this->languageDropdownOpen = false;
            this->languageScrollDragging = false;
            this->widgetDragging = false;
            this->hudScaleDragging = false;
            this->mapFrameMarginDragging = true;
            this->mapFrameMarginDragRow = index;
            this->mapFrameMarginDragGrabOffsetX = isPointInRect(x, y, thumbRect)
                                                     ? (x - thumbRect.x)
                                                     : (kHudScaleThumbWidth * 0.5f);
            this->updateDraggedMapFrameMarginFromMouse(x);
            return true;
        }

        if (isPointInRect(x, y, layout.languageButton))
        {
            this->redeemInputFocused = false;
            this->redeemInputSelectingWithMouse = false;
            this->clearRedeemSelection();
            this->languageDropdownOpen = !this->languageDropdownOpen;
            this->languageScrollDragging = false;
            this->hudScaleDragging = false;
            this->mapFrameMarginDragging = false;
            this->hoveredLanguageIndex = -1;
            this->scrollLanguageToSelection();
            return true;
        }

    }
    else if (this->activeTab == SettingsTab::CONTROLS)
    {
        this->redeemInputFocused = false;
        this->redeemInputSelectingWithMouse = false;
        this->clearRedeemSelection();
        this->languageDropdownOpen = false;
        this->languageScrollDragging = false;
        this->hudScaleDragging = false;
        this->mapFrameMarginDragging = false;

        if (this->controlConflictPending)
        {
            if (isPointInRect(x, y, layout.controlsConflictConfirmButton))
            {
                this->confirmPendingControlConflict();
                return true;
            }
            if (isPointInRect(x, y, layout.controlsConflictCancelButton))
            {
                this->cancelPendingControlConflict();
                return true;
            }
            if (isPointInRect(x, y, layout.controlsConflictPrompt))
            {
                return true;
            }
        }

        const float maxScrollOffset = getControlsMaxScrollOffset(layout);
        if (maxScrollOffset > 0.0f && isPointInRect(x, y, layout.controlsScrollTrack))
        {
            const SDL_FRect thumbRect = getControlsScrollThumbRect(layout, this->controlsScrollOffsetY);
            this->controlsScrollDragging = true;
            this->cameraScrollSpeedDragging = false;
            this->widgetDragging = false;
            if (isPointInRect(x, y, thumbRect))
            {
                this->controlsScrollDragOffsetY = y - thumbRect.y;
            }
            else
            {
                this->controlsScrollDragOffsetY = thumbRect.h * 0.5f;
                this->updateControlsScrollFromMouse(y);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.controlsResetAllButton))
        {
            this->resetAllControlActionScancodes();
            this->setCameraScrollSpeedSectors(kCameraScrollSpeedDefaultSectors);
            this->controlCaptureActive = false;
            this->controlConflictPending = false;
            return true;
        }

        if (isPointInRect(x, y, layout.controlsViewport))
        {
            float entryY = layout.controlsViewport.y - this->controlsScrollOffsetY;
            for (const GameSettingsControlsEntry& entry : kControlsEntries)
            {
                const float entryHeight = getControlsEntryHeight(entry.type);
                const SDL_FRect rowRect = SDL_FRect{
                    layout.controlsViewport.x,
                    entryY,
                    layout.controlsViewport.w,
                    entryHeight
                };

                if (isPointInRect(x, y, rowRect))
                {
                    if (entry.type == ControlsEntryType::ACTION)
                    {
                        const SDL_FRect resetButtonRect = SDL_FRect{
                            rowRect.x + rowRect.w - kControlsResetButtonWidth,
                            rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                            kControlsResetButtonWidth,
                            kControlsButtonHeight
                        };
                        const SDL_FRect keyButtonRect = SDL_FRect{
                            resetButtonRect.x - 10.0f - kControlsKeyButtonWidth,
                            rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                            kControlsKeyButtonWidth,
                            kControlsButtonHeight
                        };

                        if (isPointInRect(x, y, resetButtonRect))
                        {
                            this->resetControlActionScancode(entry.action);
                            this->controlCaptureActive = false;
                            this->controlConflictPending = false;
                            return true;
                        }
                        if (isPointInRect(x, y, keyButtonRect))
                        {
                            this->controlCaptureActive = true;
                            this->capturedControlAction = entry.action;
                            this->controlConflictPending = false;
                            return true;
                        }
                    }
                    else if (entry.type == ControlsEntryType::CAMERA_SPEED)
                    {
                        const SDL_FRect resetButtonRect = SDL_FRect{
                            rowRect.x + rowRect.w - kControlsResetButtonWidth,
                            rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                            kControlsResetButtonWidth,
                            kControlsButtonHeight
                        };
                        const SDL_FRect trackRect = SDL_FRect{
                            rowRect.x + 292.0f,
                            rowRect.y + ((rowRect.h - kControlsTrackHeight) * 0.5f),
                            (std::max)(1.0f, resetButtonRect.x - 70.0f - (rowRect.x + 292.0f)),
                            kControlsTrackHeight
                        };
                        const SDL_FRect thumbRect =
                            getCameraScrollSpeedThumbRect(trackRect, this->cameraScrollSpeedSectors);

                        if (isPointInRect(x, y, resetButtonRect))
                        {
                            this->setCameraScrollSpeedSectors(kCameraScrollSpeedDefaultSectors);
                            this->controlCaptureActive = false;
                            this->controlConflictPending = false;
                            return true;
                        }
                        if (isPointInRect(x, y, trackRect) || isPointInRect(x, y, thumbRect))
                        {
                            this->cameraScrollSpeedDragging = true;
                            this->controlsScrollDragging = false;
                            this->widgetDragging = false;
                            this->controlCaptureActive = false;
                            this->controlConflictPending = false;
                            this->cameraScrollSpeedDragGrabOffsetX =
                                isPointInRect(x, y, thumbRect)
                                    ? (x - thumbRect.x)
                                    : (kControlsThumbWidth * 0.5f);
                            this->updateDraggedCameraScrollSpeedFromMouse(x);
                            return true;
                        }
                    }

                    return true;
                }

                entryY += entryHeight + kControlsRowGap;
            }

            return true;
        }
    }
    else if (this->activeTab == SettingsTab::GRAPHICS)
    {
        this->redeemInputFocused = false;
        this->redeemInputSelectingWithMouse = false;
        this->clearRedeemSelection();
        this->languageDropdownOpen = false;
        this->languageScrollDragging = false;
        this->hudScaleDragging = false;
        this->mapFrameMarginDragging = false;
        this->controlsScrollDragging = false;
        this->cameraScrollSpeedDragging = false;
        this->controlCaptureActive = false;
        this->controlConflictPending = false;
        this->graphicsScrollDragging = false;

        const float maxGraphicsScrollOffset = getGraphicsMaxScrollOffset(layout);
        if (maxGraphicsScrollOffset > 0.0f && isPointInRect(x, y, layout.graphicsScrollTrack))
        {
            const SDL_FRect thumbRect = getGraphicsScrollThumbRect(layout, this->graphicsScrollOffsetY);
            this->graphicsScrollDragging = true;
            this->widgetDragging = false;
            if (isPointInRect(x, y, thumbRect))
            {
                this->graphicsScrollDragOffsetY = y - thumbRect.y;
            }
            else
            {
                this->graphicsScrollDragOffsetY = thumbRect.h * 0.5f;
                this->updateGraphicsScrollFromMouse(y);
            }
            return true;
        }

        const float scrollY = this->graphicsScrollOffsetY;
        const SDL_FRect graphicsWindowModeButton = offsetRect(layout.graphicsWindowModeButton, 0.0f, -scrollY);
        const SDL_FRect graphicsWindowModeDropdown = offsetRect(layout.graphicsWindowModeDropdown, 0.0f, -scrollY);
        const SDL_FRect graphicsPresentationModeButton = offsetRect(layout.graphicsPresentationModeButton, 0.0f, -scrollY);
        const SDL_FRect graphicsPresentationModeDropdown = offsetRect(layout.graphicsPresentationModeDropdown, 0.0f, -scrollY);
        const SDL_FRect graphicsVsyncRow = offsetRect(layout.graphicsVsyncRow, 0.0f, -scrollY);
        const SDL_FRect graphicsCursorLockRow = offsetRect(layout.graphicsCursorLockRow, 0.0f, -scrollY);
        const SDL_FRect graphicsHideCoordinateBackgroundRow = offsetRect(layout.graphicsHideCoordinateBackgroundRow, 0.0f, -scrollY);
        const SDL_FRect graphicsFogOfWarRow = offsetRect(layout.graphicsFogOfWarRow, 0.0f, -scrollY);
        const SDL_FRect graphicsShipWakeTrailsRow = offsetRect(layout.graphicsShipWakeTrailsRow, 0.0f, -scrollY);
        const SDL_FRect graphicsHideOtherPlayersVfxRow = offsetRect(layout.graphicsHideOtherPlayersVfxRow, 0.0f, -scrollY);

        if (this->graphicsWindowModeDropdownOpen)
        {
            for (std::size_t index = 0; index < layout.graphicsWindowModeOptions.size(); ++index)
            {
                const SDL_FRect optionRect = offsetRect(layout.graphicsWindowModeOptions[index], 0.0f, -scrollY);
                if (!isPointInRect(x, y, optionRect))
                {
                    continue;
                }

                this->applyGraphicsWindowMode(getGraphicsWindowModeForOptionIndex(index));
                this->graphicsWindowModeDropdownOpen = false;
                this->graphicsPresentationModeDropdownOpen = false;
                if (this->onUserSettingsChanged)
                {
                    this->onUserSettingsChanged();
                }
                return true;
            }

            if (isPointInRect(x, y, graphicsWindowModeDropdown))
            {
                return true;
            }

            if (!isPointInRect(x, y, graphicsWindowModeButton))
            {
                this->graphicsWindowModeDropdownOpen = false;
            }
        }

        if (this->graphicsPresentationModeDropdownOpen)
        {
            for (std::size_t index = 0; index < layout.graphicsPresentationModeOptions.size(); ++index)
            {
                const SDL_FRect optionRect = offsetRect(layout.graphicsPresentationModeOptions[index], 0.0f, -scrollY);
                if (!isPointInRect(x, y, optionRect))
                {
                    continue;
                }

                this->applyGraphicsPresentationMode(
                    index == 0U
                        ? RC2D_LOGICAL_PRESENTATION_OVERSCAN
                        : RC2D_LOGICAL_PRESENTATION_LETTERBOX);
                this->graphicsWindowModeDropdownOpen = false;
                this->graphicsPresentationModeDropdownOpen = false;
                return true;
            }

            if (isPointInRect(x, y, graphicsPresentationModeDropdown))
            {
                return true;
            }

            if (!isPointInRect(x, y, graphicsPresentationModeButton))
            {
                this->graphicsPresentationModeDropdownOpen = false;
            }
        }

        if (isPointInRect(x, y, graphicsVsyncRow))
        {
            rc2d_window_setVSync(!rc2d_window_getVSync());
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            return true;
        }

        if (isPointInRect(x, y, graphicsCursorLockRow))
        {
            rc2d_window_setMouseGrabbed(!rc2d_window_isMouseGrabbed());
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            return true;
        }

        if (isPointInRect(x, y, graphicsHideCoordinateBackgroundRow))
        {
            this->graphicsHideCoordinateBackground = !this->graphicsHideCoordinateBackground;
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            if (this->onUserSettingsChanged)
            {
                this->onUserSettingsChanged();
            }
            return true;
        }

        if (isPointInRect(x, y, graphicsFogOfWarRow))
        {
            g_graphicsFogOfWarEnabled = !g_graphicsFogOfWarEnabled;
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            if (this->onUserSettingsChanged)
            {
                this->onUserSettingsChanged();
            }
            return true;
        }

        if (isPointInRect(x, y, graphicsShipWakeTrailsRow))
        {
            g_graphicsShipWakeTrailsEnabled = !g_graphicsShipWakeTrailsEnabled;
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            if (this->onUserSettingsChanged)
            {
                this->onUserSettingsChanged();
            }
            return true;
        }

        if (isPointInRect(x, y, graphicsHideOtherPlayersVfxRow))
        {
            this->graphicsHideOtherPlayersVfx = !this->graphicsHideOtherPlayersVfx;
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            if (this->onUserSettingsChanged)
            {
                this->onUserSettingsChanged();
            }
            return true;
        }

        for (std::size_t index = 0; index < layout.graphicsSalvoOptions.size(); ++index)
        {
            const SDL_FRect optionRect = offsetRect(layout.graphicsSalvoOptions[index], 0.0f, -scrollY);
            if (!isPointInRect(x, y, optionRect))
            {
                continue;
            }
            this->graphicsSalvoBulletPreset = static_cast<SalvoBulletPreset>(static_cast<int>(index));
            this->graphicsWindowModeDropdownOpen = false;
            this->graphicsPresentationModeDropdownOpen = false;
            if (this->onUserSettingsChanged)
            {
                this->onUserSettingsChanged();
            }
            return true;
        }

        if (isPointInRect(x, y, graphicsWindowModeButton))
        {
            this->graphicsWindowModeDropdownOpen = !this->graphicsWindowModeDropdownOpen;
            this->graphicsPresentationModeDropdownOpen = false;
            return true;
        }

        if (isPointInRect(x, y, graphicsPresentationModeButton))
        {
            this->graphicsPresentationModeDropdownOpen = !this->graphicsPresentationModeDropdownOpen;
            this->graphicsWindowModeDropdownOpen = false;
            return true;
        }
    }

    if (isPointInRect(x, y, layout.headerDragRect))
    {
        this->widgetDragging = true;
        this->languageScrollDragging = false;
        this->hudScaleDragging = false;
        this->mapFrameMarginDragging = false;
        this->controlsScrollDragging = false;
        this->cameraScrollSpeedDragging = false;
        this->graphicsWindowModeDropdownOpen = false;
        this->graphicsPresentationModeDropdownOpen = false;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->redeemInputFocused = false;
        this->controlCaptureActive = false;
        return true;
    }

    if (this->activeTab != SettingsTab::GENERAL || !isPointInRect(x, y, layout.redeemInput))
    {
        this->redeemInputFocused = false;
        this->redeemInputSelectingWithMouse = false;
        this->clearRedeemSelection();
    }
    this->hudScaleDragging = false;
    this->mapFrameMarginDragging = false;
    this->controlsScrollDragging = false;
    this->cameraScrollSpeedDragging = false;
    return true;
}

bool GameSettingsWidget::mousewheelmoved(
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

    if (!this->visible)
    {
        return false;
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
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

    if (this->activeTab == SettingsTab::CONTROLS && isPointInRect(mouse_x, mouse_y, layout.controlsPanel))
    {
        if (delta != 0)
        {
            const float beforeControlsScroll = this->controlsScrollOffsetY;
            this->controlsScrollOffsetY -= static_cast<float>(delta) * kControlsWheelStep;
            this->clampControlsScroll();
            if (this->controlsScrollOffsetY != beforeControlsScroll)
            {
                this->controlsScrollWheelHighlightSec = kScrollThumbWheelHighlightSec;
            }
        }
        return true;
    }

    if (this->activeTab == SettingsTab::GRAPHICS && isPointInRect(mouse_x, mouse_y, layout.graphicsPanel))
    {
        if (delta != 0)
        {
            const float beforeGraphicsScroll = this->graphicsScrollOffsetY;
            this->graphicsScrollOffsetY -= static_cast<float>(delta) * kGraphicsWheelStep;
            this->clampGraphicsScroll();
            if (this->graphicsScrollOffsetY != beforeGraphicsScroll)
            {
                this->graphicsScrollWheelHighlightSec = kScrollThumbWheelHighlightSec;
            }
        }
        return true;
    }

    if (!this->languageDropdownOpen || !isPointInRect(mouse_x, mouse_y, layout.languageDropdown))
    {
        return false;
    }

    const int visibleRows = this->getVisibleLanguageRowCount();
    const int maxFirstRow = getLanguageMaxFirstRow(static_cast<int>(this->languageOptions.size()), visibleRows);
    if (maxFirstRow <= 0)
    {
        this->languageFirstRow = 0;
        return true;
    }

    if (delta != 0)
    {
        const int beforeLanguageRow = this->languageFirstRow;
        this->languageFirstRow -= delta;
        this->clampLanguageScroll();
        if (this->languageFirstRow != beforeLanguageRow)
        {
            this->languageScrollWheelHighlightSec = kScrollThumbWheelHighlightSec;
        }
    }
    return true;
}

bool GameSettingsWidget::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    if (this->visible && this->activeTab == SettingsTab::CONTROLS && this->controlCaptureActive)
    {
        if (isrepeat)
        {
            return true;
        }
        if (scancode == SDL_SCANCODE_ESCAPE)
        {
            this->controlCaptureActive = false;
            this->controlConflictPending = false;
            return true;
        }
        if (isBindableScancode(scancode))
        {
            ControlAction conflictAction = ControlAction::COUNT;
            if (this->findControlActionUsingScancode(scancode, this->capturedControlAction, &conflictAction))
            {
                this->beginControlConflictConfirmation(this->capturedControlAction, conflictAction, scancode);
            }
            else
            {
                this->applyControlActionScancode(this->capturedControlAction, scancode);
                this->controlConflictPending = false;
            }
            this->controlCaptureActive = false;
        }
        return true;
    }

    if (!this->visible || !this->redeemInputFocused)
    {
        return false;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::UNDO, key, scancode, keycode, mod, isrepeat))
    {
        if (this->redeemEditHistory.undo(
                &this->redeemCode,
                &this->redeemCursorIndex,
                &this->redeemSelectionAnchorIndex))
        {
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
        }
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::SELECT_ALL, key, scancode, keycode, mod, isrepeat))
    {
        this->redeemSelectionAnchorIndex = 0U;
        this->redeemCursorIndex = this->redeemCode.size();
        this->redeemCursorVisible = true;
        this->redeemCursorBlinkElapsed = 0.0;
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::COPY, key, scancode, keycode, mod, isrepeat))
    {
        if (this->hasRedeemSelection())
        {
            const std::size_t selectionStart = this->getRedeemSelectionStart();
            const std::size_t selectionEnd = this->getRedeemSelectionEnd();
            rc2d_system_setClipboardText(
                this->redeemCode.substr(selectionStart, selectionEnd - selectionStart).c_str());
        }
        return true;
    }

    if (isTextInputShortcutPressed(TextInputShortcut::PASTE, key, scancode, keycode, mod, isrepeat))
    {
        char* clipboardText = rc2d_system_getClipboardText();
        if (clipboardText != nullptr)
        {
            std::string sanitized;
            sanitized.reserve(kMaxRedeemCodeLength);
            for (const char* cursor = clipboardText; *cursor != '\0'; ++cursor)
            {
                const unsigned char character = static_cast<unsigned char>(*cursor);
                if (std::isalnum(character) != 0 || *cursor == '-' || *cursor == '_')
                {
                    sanitized.push_back(static_cast<char>(std::toupper(character)));
                }
            }

            if (!sanitized.empty())
            {
                this->redeemEditHistory.rememberState(
                    this->redeemCode,
                    this->redeemCursorIndex,
                    this->redeemSelectionAnchorIndex);
                if (this->hasRedeemSelection())
                {
                    this->deleteSelectedRedeemText();
                }

                const std::size_t availableCount = kMaxRedeemCodeLength - this->redeemCode.size();
                sanitized.resize((std::min)(sanitized.size(), availableCount));
                this->redeemCursorIndex = (std::min)(this->redeemCursorIndex, this->redeemCode.size());
                this->redeemCode.insert(this->redeemCursorIndex, sanitized);
                this->redeemCursorIndex += sanitized.size();
                this->clearRedeemSelection();
                this->redeemCursorVisible = true;
                this->redeemCursorBlinkElapsed = 0.0;
            }

            rc2d_system_freeClipboardText(clipboardText);
        }
        return true;
    }

    switch (keycode)
    {
        case SDLK_BACKSPACE:
            if (this->hasRedeemSelection())
            {
                this->redeemEditHistory.rememberState(
                    this->redeemCode,
                    this->redeemCursorIndex,
                    this->redeemSelectionAnchorIndex);
                this->deleteSelectedRedeemText();
            }
            else if (this->redeemCursorIndex > 0U && !this->redeemCode.empty())
            {
                this->redeemEditHistory.rememberState(
                    this->redeemCode,
                    this->redeemCursorIndex,
                    this->redeemSelectionAnchorIndex);
                this->redeemCode.erase(this->redeemCursorIndex - 1U, 1U);
                --this->redeemCursorIndex;
            }
            this->clearRedeemSelection();
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            return true;
        case SDLK_DELETE:
            if (this->hasRedeemSelection())
            {
                this->redeemEditHistory.rememberState(
                    this->redeemCode,
                    this->redeemCursorIndex,
                    this->redeemSelectionAnchorIndex);
                this->deleteSelectedRedeemText();
            }
            else if (this->redeemCursorIndex < this->redeemCode.size())
            {
                this->redeemEditHistory.rememberState(
                    this->redeemCode,
                    this->redeemCursorIndex,
                    this->redeemSelectionAnchorIndex);
                this->redeemCode.erase(this->redeemCursorIndex, 1U);
            }
            this->clearRedeemSelection();
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            return true;
        case SDLK_LEFT:
            if (this->hasRedeemSelection())
            {
                this->redeemCursorIndex = this->getRedeemSelectionStart();
            }
            else if (this->redeemCursorIndex > 0U)
            {
                --this->redeemCursorIndex;
            }
            this->clearRedeemSelection();
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            return true;
        case SDLK_RIGHT:
            if (this->hasRedeemSelection())
            {
                this->redeemCursorIndex = this->getRedeemSelectionEnd();
            }
            else if (this->redeemCursorIndex < this->redeemCode.size())
            {
                ++this->redeemCursorIndex;
            }
            this->clearRedeemSelection();
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            return true;
        case SDLK_HOME:
            this->redeemCursorIndex = 0U;
            this->clearRedeemSelection();
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            return true;
        case SDLK_END:
            this->redeemCursorIndex = this->redeemCode.size();
            this->clearRedeemSelection();
            this->redeemCursorVisible = true;
            this->redeemCursorBlinkElapsed = 0.0;
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            this->triggerRedeemCodeRequest();
            return true;
        case SDLK_ESCAPE:
            this->redeemInputFocused = false;
            this->redeemInputSelectingWithMouse = false;
            this->clearRedeemSelection();
            return true;
        default:
            break;
    }

    return false;
}

bool GameSettingsWidget::textinput(const char* text)
{
    if (!this->visible || !this->redeemInputFocused)
    {
        return false;
    }

    const std::string sanitized = sanitizeRedeemCodeValue(text != nullptr ? text : "");
    if (sanitized.empty())
    {
        return true;
    }

    this->redeemEditHistory.rememberState(
        this->redeemCode,
        this->redeemCursorIndex,
        this->redeemSelectionAnchorIndex);
    if (this->hasRedeemSelection())
    {
        this->deleteSelectedRedeemText();
    }

    const std::size_t availableCount = kMaxRedeemCodeLength - this->redeemCode.size();
    std::string clamped = sanitized.substr(0U, availableCount);
    this->redeemCursorIndex = (std::min)(this->redeemCursorIndex, this->redeemCode.size());
    this->redeemCode.insert(this->redeemCursorIndex, clamped);
    this->redeemCursorIndex += clamped.size();
    this->clearRedeemSelection();
    this->redeemCursorVisible = true;
    this->redeemCursorBlinkElapsed = 0.0;
    return true;
}

void GameSettingsWidget::draw(void) const
{
    GameSettingsWidget* self = const_cast<GameSettingsWidget*>(this);
    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    self->widgetRect = SDL_FRect{
        baseRect.x + self->widgetOffsetX,
        baseRect.y + self->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    if (!self->visible)
    {
        return;
    }

    const GameSettingsLayout layout = buildLayout(self->widgetRect, static_cast<int>(self->languageOptions.size()));
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    rc2d_graphics_setColor(kPanelFill);
    rc2d_graphics_rectangle("fill", &layout.outer);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.outer);
    rc2d_graphics_setColor(kSilver);
    rc2d_graphics_rectangle("line", &layout.inner);

    rc2d_graphics_setColor(kHeaderFill);
    rc2d_graphics_rectangle("fill", &layout.topBar);
    rc2d_graphics_setColor(kGold);
    rc2d_graphics_rectangle("line", &layout.topBar);

    const std::array<const char*, 4> tabLabels = {"General", "Controles", "Graphiques", "Sons"};
    for (int tabIndex = 0; tabIndex < static_cast<int>(layout.tabs.size()); ++tabIndex)
    {
        const bool active = self->activeTab == static_cast<SettingsTab>(tabIndex);
        rc2d_graphics_setColor(active ? kTabActive : kTabInactive);
        rc2d_graphics_rectangle("fill", &layout.tabs[static_cast<std::size_t>(tabIndex)]);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.tabs[static_cast<std::size_t>(tabIndex)]);
        drawCentered(
            &self->smallFont,
            tabLabels[static_cast<std::size_t>(tabIndex)],
            layout.tabs[static_cast<std::size_t>(tabIndex)],
            active ? kTextGold : kTextBody);
    }

    self->controlIcons.drawCloseButton(layout.closeButton, kHeaderFill, kGold);

    if (self->activeTab == SettingsTab::GENERAL)
    {
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.redeemPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.redeemPanel);
        drawTextAt(&self->titleFont, "Code d'activation", layout.redeemPanel.x + 16.0f, layout.redeemPanel.y + 14.0f, kTextGold);
        drawWrappedText(
            &self->bodyFont,
            "Saisissez un code et recevez instantanement vos recompenses.",
            SDL_FRect{layout.redeemPanel.x + 16.0f, layout.redeemPanel.y + 48.0f, layout.redeemPanel.w - 32.0f, 26.0f},
            kTextBody,
            3.0f);

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &layout.redeemInput);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.redeemInput);

        const SDL_FRect redeemTextRect = SDL_FRect{
            layout.redeemInput.x + 10.0f,
            layout.redeemInput.y + 7.0f,
            layout.redeemInput.w - 20.0f,
            layout.redeemInput.h - 14.0f
        };
        SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
        if (renderer != nullptr)
        {
            const SDL_Rect clipRect = toClipRect(redeemTextRect);
            SDL_SetRenderClipRect(renderer, &clipRect);
        }

        if (self->redeemCode.empty() && !self->redeemInputFocused)
        {
            drawLeftCenteredY(&self->bodyFont, "Entrez un code", layout.redeemInput, layout.redeemInput.x + 10.0f, kTextMuted);
        }
        else
        {
            if (self->hasRedeemSelection())
            {
                const std::size_t selectionStart = self->getRedeemSelectionStart();
                const std::size_t selectionEnd = self->getRedeemSelectionEnd();
                const std::string beforeSelection = self->redeemCode.substr(0, selectionStart);
                const std::string selectedText = self->redeemCode.substr(selectionStart, selectionEnd - selectionStart);
                const float selectionX = std::round(layout.redeemInput.x + 10.0f + measureTextWidth(&self->bodyFont, beforeSelection));
                const float selectionW = measureTextWidth(&self->bodyFont, selectedText);
                const SDL_FRect selectionRect = SDL_FRect{
                    selectionX - 1.0f,
                    layout.redeemInput.y + 7.0f,
                    (std::max)(2.0f, selectionW + 2.0f),
                    layout.redeemInput.h - 14.0f
                };
                rc2d_graphics_setColor(kInputSelectionFill);
                rc2d_graphics_rectangle("fill", &selectionRect);
            }
            drawLeftCenteredY(&self->bodyFont, self->redeemCode, layout.redeemInput, layout.redeemInput.x + 10.0f, kTextBody);
            if (self->hasRedeemSelection())
            {
                const std::size_t selectionStart = self->getRedeemSelectionStart();
                const std::size_t selectionEnd = self->getRedeemSelectionEnd();
                const std::string beforeSelection = self->redeemCode.substr(0, selectionStart);
                const std::string selectedText = self->redeemCode.substr(selectionStart, selectionEnd - selectionStart);
                const float selectionX = std::round(layout.redeemInput.x + 10.0f + measureTextWidth(&self->bodyFont, beforeSelection));
                drawLeftCenteredY(&self->bodyFont, selectedText, layout.redeemInput, selectionX, kTextBody);
            }
        }

        if (self->redeemInputFocused && self->redeemCursorVisible)
        {
            const std::string prefix = self->redeemCode.substr(0U, self->redeemCursorIndex);
            const float cursorX = layout.redeemInput.x + 10.0f + measureTextWidth(&self->bodyFont, prefix);
            const SDL_FRect cursorRect = SDL_FRect{
                cursorX,
                layout.redeemInput.y + 7.0f,
                1.5f,
                layout.redeemInput.h - 14.0f
            };
            rc2d_graphics_setColor(kTextGold);
            rc2d_graphics_rectangle("fill", &cursorRect);
        }

        if (renderer != nullptr)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
        }

        const bool redeemButtonHovered = isPointInRect(mouseX, mouseY, layout.redeemButton);
        rc2d_graphics_setColor(redeemButtonHovered ? kButtonFillHover : kButtonFillActive);
        rc2d_graphics_rectangle("fill", &layout.redeemButton);
        rc2d_graphics_setColor(redeemButtonHovered ? kSliderThumbBorder : kGold);
        rc2d_graphics_rectangle("line", &layout.redeemButton);
        drawCentered(&self->smallFont, "Activer", layout.redeemButton, kTextGold);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.configuratorPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.configuratorPanel);
        drawTextAt(&self->titleFont, "Configurateur d'interface", layout.configuratorPanel.x + 16.0f, layout.configuratorPanel.y + 14.0f, kTextGold);
        drawWrappedText(
            &self->bodyFont,
            "Le configurateur d'interface vous permet d'organiser et de deplacer l'interface comme vous le souhaitez.",
            SDL_FRect{layout.configuratorPanel.x + 16.0f, layout.configuratorPanel.y + 46.0f, layout.configuratorPanel.w - 32.0f, 30.0f},
            kTextBody,
            3.0f);

        const bool configuratorButtonHovered = isPointInRect(mouseX, mouseY, layout.configuratorButton);
        rc2d_graphics_setColor(configuratorButtonHovered ? kButtonFillHover : kButtonFillActive);
        rc2d_graphics_rectangle("fill", &layout.configuratorButton);
        rc2d_graphics_setColor(configuratorButtonHovered ? kSliderThumbBorder : kGold);
        rc2d_graphics_rectangle("line", &layout.configuratorButton);
        drawCentered(
            &self->smallFont,
            "Demarrer la configuration d'interface",
            layout.configuratorButton,
            kTextGold);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.hudScalePanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.hudScalePanel);
        drawTextAt(&self->titleFont, "Changer la taille ou masquer les interfaces", layout.hudScalePanel.x + 16.0f, layout.hudScalePanel.y + 14.0f, kTextGold);
        drawWrappedText(
            &self->bodyFont,
            "Ajustez la taille de chaque interface ou masquez-la pour gagner de la place a l'ecran.",
            SDL_FRect{layout.hudScalePanel.x + 16.0f, layout.hudScalePanel.y + 38.0f, layout.hudScalePanel.w - 32.0f, 24.0f},
            kTextBody,
            3.0f);

        for (std::size_t index = 0; index < layout.hudScaleRows.size(); ++index)
        {
            const SDL_FRect& rowRect = layout.hudScaleRows[index];
            const SDL_FRect& trackRect = layout.hudScaleTracks[index];
            const SDL_FRect& visibilityButtonRect = layout.hudScaleVisibilityButtons[index];
            const HudScaleTarget target = static_cast<HudScaleTarget>(index);
            const SDL_FRect thumbRect = getHudScaleThumbRect(trackRect, target, self->hudScaleValues[index]);
            const float thumbCenterX = thumbRect.x + (thumbRect.w * 0.5f);
            const SDL_FRect activeRect = SDL_FRect{
                trackRect.x,
                trackRect.y,
                (std::max)(0.0f, thumbCenterX - trackRect.x),
                trackRect.h
            };
            const bool widgetVisible = self->hudVisibilityValues[index];
            const bool visibilityButtonHovered = isPointInRect(mouseX, mouseY, visibilityButtonRect);

            char scaleLabel[16] = {};
            SDL_snprintf(
                scaleLabel,
                sizeof(scaleLabel),
                "%d%%",
                static_cast<int>(std::round(self->hudScaleValues[index] * 100.0f)));

            drawLeftCenteredY(
                &self->bodyFont,
                kHudScaleLabels[index],
                rowRect,
                rowRect.x,
                widgetVisible ? kTextBody : kTextMuted);

            rc2d_graphics_setColor(kSliderTrackFill);
            rc2d_graphics_rectangle("fill", &trackRect);
            if (activeRect.w > 0.0f)
            {
                rc2d_graphics_setColor(widgetVisible ? kSliderActiveFill : kSliderInactiveFill);
                rc2d_graphics_rectangle("fill", &activeRect);
            }
            rc2d_graphics_setColor(kRowLine);
            rc2d_graphics_rectangle("line", &trackRect);

            rc2d_graphics_setColor(widgetVisible ? kSliderThumbFill : kButtonFillMuted);
            rc2d_graphics_rectangle("fill", &thumbRect);
            rc2d_graphics_setColor(kSliderThumbBorder);
            rc2d_graphics_rectangle("line", &thumbRect);

            drawLeftCenteredY(
                &self->smallFont,
                scaleLabel,
                rowRect,
                visibilityButtonRect.x - 12.0f - kHudScaleValueLabelWidth,
                widgetVisible ? kTextGold : kTextMuted);

            rc2d_graphics_setColor(
                visibilityButtonHovered
                    ? kButtonFillHover
                    : (widgetVisible ? kButtonFillActive : kButtonFillMuted));
            rc2d_graphics_rectangle("fill", &visibilityButtonRect);
            rc2d_graphics_setColor(visibilityButtonHovered ? kSliderThumbBorder : kGold);
            rc2d_graphics_rectangle("line", &visibilityButtonRect);
            drawCentered(
                &self->smallFont,
                widgetVisible ? "Masquer" : "Afficher",
                visibilityButtonRect,
                visibilityButtonHovered ? kTextGold : (                widgetVisible ? kTextGold : kTextBody));
        }

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.mapFramePanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.mapFramePanel);
        drawTextAt(
            &self->titleFont,
            "Mise en cadre de la zone map",
            layout.mapFramePanel.x + 16.0f,
            layout.mapFramePanel.y + 14.0f,
            kTextGold);
        drawWrappedText(
            &self->bodyFont,
            "Reservez des bandes hors carte (jusqu'a 20 %, reglage au pourcent pres). La marge au-dessus de la map reste fixe (25 px).",
            SDL_FRect{layout.mapFramePanel.x + 16.0f, layout.mapFramePanel.y + 38.0f, layout.mapFramePanel.w - 32.0f, 34.0f},
            kTextBody,
            3.0f);

        for (std::size_t index = 0; index < layout.mapFrameRows.size(); ++index)
        {
            const SDL_FRect& rowRect = layout.mapFrameRows[index];
            const SDL_FRect& trackRect = layout.mapFrameTracks[index];
            const int marginPct = self->mapPlayfieldFrameMarginPercent[index];
            const SDL_FRect thumbRect = getMapFrameMarginThumbRect(trackRect, marginPct);
            const float thumbCenterX = thumbRect.x + (thumbRect.w * 0.5f);
            const SDL_FRect activeRect = SDL_FRect{
                trackRect.x,
                trackRect.y,
                (std::max)(0.0f, thumbCenterX - trackRect.x),
                trackRect.h
            };

            char pctLabel[16] = {};
            SDL_snprintf(pctLabel, sizeof(pctLabel), "%d %%", marginPct);

            drawLeftCenteredY(
                &self->bodyFont,
                kMapFrameMarginRowLabels[index],
                rowRect,
                rowRect.x,
                kTextBody);

            rc2d_graphics_setColor(kSliderTrackFill);
            rc2d_graphics_rectangle("fill", &trackRect);
            if (activeRect.w > 0.0f)
            {
                rc2d_graphics_setColor(kSliderActiveFill);
                rc2d_graphics_rectangle("fill", &activeRect);
            }
            rc2d_graphics_setColor(kRowLine);
            rc2d_graphics_rectangle("line", &trackRect);

            rc2d_graphics_setColor(kSliderThumbFill);
            rc2d_graphics_rectangle("fill", &thumbRect);
            rc2d_graphics_setColor(kSliderThumbBorder);
            rc2d_graphics_rectangle("line", &thumbRect);

            const float valueX = rowRect.x + rowRect.w - kHudScaleValueLabelWidth;
            drawLeftCenteredY(&self->smallFont, pctLabel, rowRect, valueX, kTextGold);
        }

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.languageRow);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.languageRow);
        drawLeftCenteredY(&self->titleFont, "Langues", layout.languageRow, layout.languageRow.x + 16.0f, kTextGold);

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &layout.languageButton);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.languageButton);

        std::string selectedLanguageLabel = "Anglais";
        if (self->selectedLanguageIndex >= 0 &&
            self->selectedLanguageIndex < static_cast<int>(self->languageOptions.size()))
        {
            selectedLanguageLabel = self->languageOptions[static_cast<std::size_t>(self->selectedLanguageIndex)].label;
        }
        drawLeftCenteredY(
            &self->bodyFont,
            selectedLanguageLabel,
            layout.languageButton,
            layout.languageButton.x + 10.0f,
            kTextGold);

        const SDL_FRect arrowRect = SDL_FRect{
            layout.languageButton.x + layout.languageButton.w - 22.0f,
            layout.languageButton.y + ((layout.languageButton.h - 14.0f) * 0.5f),
            14.0f,
            14.0f
        };
        drawImageFit(self->arrowDownIcon, arrowRect, 0.0f);

        if (self->languageDropdownOpen)
        {
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &layout.languageDropdown);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &layout.languageDropdown);

            const int visibleRows = self->getVisibleLanguageRowCount();
            const int maxFirstRow = getLanguageMaxFirstRow(static_cast<int>(self->languageOptions.size()), visibleRows);
            for (int row = 0; row < visibleRows; ++row)
            {
                const int optionIndex = self->languageFirstRow + row;
                if (optionIndex < 0 || optionIndex >= static_cast<int>(self->languageOptions.size()))
                {
                    break;
                }

                const SDL_FRect rowRect = SDL_FRect{
                    layout.languageViewport.x,
                    layout.languageViewport.y + (static_cast<float>(row) * kLanguageRowHeight),
                    layout.languageViewport.w,
                    kLanguageRowHeight
                };
                const bool selected = optionIndex == self->selectedLanguageIndex;
                const bool hovered = optionIndex == self->hoveredLanguageIndex;
                if (selected || hovered)
                {
                    rc2d_graphics_setColor(selected ? kTabActive : kSelectionFill);
                    rc2d_graphics_rectangle("fill", &rowRect);
                }
                rc2d_graphics_setColor(kRowLine);
                rc2d_graphics_rectangle("line", &rowRect);
                drawLeftCenteredY(
                    &self->bodyFont,
                    self->languageOptions[static_cast<std::size_t>(optionIndex)].label,
                    rowRect,
                    rowRect.x + 10.0f,
                    selected ? kTextGold : kTextBody);
            }

            if (maxFirstRow > 0)
            {
                const float thumbHeight = (std::max)(
                    kMinThumbHeight,
                    layout.languageScrollTrack.h *
                        (static_cast<float>(visibleRows) /
                         static_cast<float>((std::max)(1, static_cast<int>(self->languageOptions.size())))));
                const float thumbTravel = (std::max)(1.0f, layout.languageScrollTrack.h - thumbHeight);
                const float scrollRatio = static_cast<float>(self->languageFirstRow) / static_cast<float>(maxFirstRow);
                const SDL_FRect thumbRect = SDL_FRect{
                    layout.languageScrollTrack.x,
                    layout.languageScrollTrack.y + (thumbTravel * scrollRatio),
                    layout.languageScrollTrack.w,
                    thumbHeight
                };

                rc2d_graphics_setColor(kScrollTrack);
                rc2d_graphics_rectangle("fill", &layout.languageScrollTrack);
                rc2d_graphics_setColor(kRowLine);
                rc2d_graphics_rectangle("line", &layout.languageScrollTrack);
                rc2d_graphics_setColor((self->languageScrollDragging || (self->languageScrollWheelHighlightSec > 0.0f)) ? kSliderThumbFill : kScrollThumb);
                rc2d_graphics_rectangle("fill", &thumbRect);
            }
        }
    }
    else if (self->activeTab == SettingsTab::CONTROLS)
    {
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.controlsPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.controlsPanel);

        self->controlsScrollOffsetY =
            clampf(self->controlsScrollOffsetY, 0.0f, getControlsMaxScrollOffset(layout));

        SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
        if (renderer != nullptr)
        {
            const SDL_Rect clipRect = toClipRect(layout.controlsViewport);
            SDL_SetRenderClipRect(renderer, &clipRect);
        }

        float entryY = layout.controlsViewport.y - self->controlsScrollOffsetY;
        for (const GameSettingsControlsEntry& entry : kControlsEntries)
        {
            const float entryHeight = getControlsEntryHeight(entry.type);
            const SDL_FRect rowRect = SDL_FRect{
                layout.controlsViewport.x,
                entryY,
                layout.controlsViewport.w,
                entryHeight
            };

            const bool visibleRow =
                rowRect.y + rowRect.h >= layout.controlsViewport.y &&
                rowRect.y <= layout.controlsViewport.y + layout.controlsViewport.h;
            if (visibleRow)
            {
                if (entry.type == ControlsEntryType::SECTION)
                {
                    drawLeftCenteredY(
                        &self->titleFont,
                        entry.label,
                        rowRect,
                        rowRect.x,
                        kTextGold);
                    rc2d_graphics_setColor(kRowLine);
                    rc2d_graphics_line(
                        rowRect.x,
                        rowRect.y + rowRect.h - 2.0f,
                        rowRect.x + rowRect.w,
                        rowRect.y + rowRect.h - 2.0f);
                }
                else if (entry.type == ControlsEntryType::ACTION)
                {
                    const SDL_FRect resetButtonRect = SDL_FRect{
                        rowRect.x + rowRect.w - kControlsResetButtonWidth,
                        rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                        kControlsResetButtonWidth,
                        kControlsButtonHeight
                    };
                    const SDL_FRect keyButtonRect = SDL_FRect{
                        resetButtonRect.x - 10.0f - kControlsKeyButtonWidth,
                        rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                        kControlsKeyButtonWidth,
                        kControlsButtonHeight
                    };
                    const bool keyHovered = isPointInRect(mouseX, mouseY, keyButtonRect);
                    const bool resetHovered = isPointInRect(mouseX, mouseY, resetButtonRect);
                    const bool capturing =
                        self->controlCaptureActive &&
                        self->capturedControlAction == entry.action;

                    rc2d_graphics_setColor(kSelectionFill);
                    rc2d_graphics_rectangle("line", &rowRect);
                    drawLeftCenteredY(
                        &self->bodyFont,
                        entry.label,
                        rowRect,
                        rowRect.x + 4.0f,
                        kTextBody);

                    rc2d_graphics_setColor(keyHovered || capturing ? kButtonFillHover : kFieldFill);
                    rc2d_graphics_rectangle("fill", &keyButtonRect);
                    rc2d_graphics_setColor(keyHovered || capturing ? kSliderThumbBorder : kGold);
                    rc2d_graphics_rectangle("line", &keyButtonRect);
                    drawCentered(
                        &self->smallFont,
                        capturing ? "Appuyez..." : self->getControlActionLabel(entry.action).c_str(),
                        keyButtonRect,
                        capturing ? kTextGold : kTextBody);

                    rc2d_graphics_setColor(resetHovered ? kButtonFillHover : kButtonFillMuted);
                    rc2d_graphics_rectangle("fill", &resetButtonRect);
                    rc2d_graphics_setColor(resetHovered ? kSliderThumbBorder : kGold);
                    rc2d_graphics_rectangle("line", &resetButtonRect);
                    drawCentered(&self->smallFont, "Réinitialisation", resetButtonRect, kTextGold);
                }
                else if (entry.type == ControlsEntryType::CAMERA_SPEED)
                {
                    const SDL_FRect resetButtonRect = SDL_FRect{
                        rowRect.x + rowRect.w - kControlsResetButtonWidth,
                        rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                        kControlsResetButtonWidth,
                        kControlsButtonHeight
                    };
                    const SDL_FRect trackRect = SDL_FRect{
                        rowRect.x + 292.0f,
                        rowRect.y + ((rowRect.h - kControlsTrackHeight) * 0.5f),
                        (std::max)(1.0f, resetButtonRect.x - 70.0f - (rowRect.x + 292.0f)),
                        kControlsTrackHeight
                    };
                    const SDL_FRect thumbRect =
                        getCameraScrollSpeedThumbRect(trackRect, self->cameraScrollSpeedSectors);
                    const float thumbCenterX = thumbRect.x + (thumbRect.w * 0.5f);
                    const SDL_FRect activeRect = SDL_FRect{
                        trackRect.x,
                        trackRect.y,
                        (std::max)(0.0f, thumbCenterX - trackRect.x),
                        trackRect.h
                    };
                    const bool resetHovered = isPointInRect(mouseX, mouseY, resetButtonRect);

                    rc2d_graphics_setColor(kSelectionFill);
                    rc2d_graphics_rectangle("line", &rowRect);
                    drawLeftCenteredY(
                        &self->bodyFont,
                        entry.label,
                        rowRect,
                        rowRect.x + 4.0f,
                        kTextBody);

                    rc2d_graphics_setColor(kSliderTrackFill);
                    rc2d_graphics_rectangle("fill", &trackRect);
                    rc2d_graphics_setColor(kSliderActiveFill);
                    rc2d_graphics_rectangle("fill", &activeRect);
                    rc2d_graphics_setColor(kRowLine);
                    rc2d_graphics_rectangle("line", &trackRect);

                    rc2d_graphics_setColor(kSliderThumbFill);
                    rc2d_graphics_rectangle("fill", &thumbRect);
                    rc2d_graphics_setColor(kSliderThumbBorder);
                    rc2d_graphics_rectangle("line", &thumbRect);

                    char speedLabel[24] = {};
                    SDL_snprintf(
                        speedLabel,
                        sizeof(speedLabel),
                        "%.1f",
                        static_cast<double>(self->cameraScrollSpeedSectors));
                    drawLeftCenteredY(
                        &self->smallFont,
                        speedLabel,
                        rowRect,
                        resetButtonRect.x - 58.0f,
                        kTextGold);

                    rc2d_graphics_setColor(resetHovered ? kButtonFillHover : kButtonFillMuted);
                    rc2d_graphics_rectangle("fill", &resetButtonRect);
                    rc2d_graphics_setColor(resetHovered ? kSliderThumbBorder : kGold);
                    rc2d_graphics_rectangle("line", &resetButtonRect);
                    drawCentered(&self->smallFont, "Réinitialisation", resetButtonRect, kTextGold);
                }
            }

            entryY += entryHeight + kControlsRowGap;
        }

        if (renderer != nullptr)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
        }

        const float maxControlsScrollOffset = getControlsMaxScrollOffset(layout);
        if (maxControlsScrollOffset > 0.0f)
        {
            const SDL_FRect thumbRect = getControlsScrollThumbRect(layout, self->controlsScrollOffsetY);
            rc2d_graphics_setColor(kScrollTrack);
            rc2d_graphics_rectangle("fill", &layout.controlsScrollTrack);
            rc2d_graphics_setColor(kRowLine);
            rc2d_graphics_rectangle("line", &layout.controlsScrollTrack);
            rc2d_graphics_setColor((self->controlsScrollDragging || (self->controlsScrollWheelHighlightSec > 0.0f)) ? kSliderThumbFill : kScrollThumb);
            rc2d_graphics_rectangle("fill", &thumbRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &thumbRect);
        }

        if (self->controlConflictPending)
        {
            rc2d_graphics_setColor(kSelectionFill);
            rc2d_graphics_rectangle("fill", &layout.controlsConflictPrompt);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &layout.controlsConflictPrompt);

            const std::string warningText =
                std::string("Touche ") +
                getDisplayNameForScancode(self->pendingConflictScancode) +
                " deja utilisee par " +
                getControlActionDisplayName(self->conflictingControlAction) +
                ". Remplacer ?";
            const SDL_FRect warningTextRect = SDL_FRect{
                layout.controlsConflictPrompt.x,
                layout.controlsConflictPrompt.y,
                layout.controlsConflictConfirmButton.x - layout.controlsConflictPrompt.x - 8.0f,
                layout.controlsConflictPrompt.h
            };
            if (renderer != nullptr)
            {
                const SDL_Rect clipRect = toClipRect(warningTextRect);
                SDL_SetRenderClipRect(renderer, &clipRect);
            }
            drawLeftCenteredY(
                &self->smallFont,
                warningText,
                warningTextRect,
                warningTextRect.x + 8.0f,
                kTextGold);
            if (renderer != nullptr)
            {
                SDL_SetRenderClipRect(renderer, nullptr);
            }

            const bool confirmHovered = isPointInRect(mouseX, mouseY, layout.controlsConflictConfirmButton);
            rc2d_graphics_setColor(confirmHovered ? kButtonFillHover : kButtonFillActive);
            rc2d_graphics_rectangle("fill", &layout.controlsConflictConfirmButton);
            rc2d_graphics_setColor(confirmHovered ? kSliderThumbBorder : kGold);
            rc2d_graphics_rectangle("line", &layout.controlsConflictConfirmButton);
            drawCentered(&self->smallFont, "Confirmer", layout.controlsConflictConfirmButton, kTextGold);

            const bool cancelHovered = isPointInRect(mouseX, mouseY, layout.controlsConflictCancelButton);
            rc2d_graphics_setColor(cancelHovered ? kButtonFillHover : kButtonFillMuted);
            rc2d_graphics_rectangle("fill", &layout.controlsConflictCancelButton);
            rc2d_graphics_setColor(cancelHovered ? kSliderThumbBorder : kGold);
            rc2d_graphics_rectangle("line", &layout.controlsConflictCancelButton);
            drawCentered(&self->smallFont, "Annuler", layout.controlsConflictCancelButton, kTextGold);
        }

        const bool resetAllHovered = isPointInRect(mouseX, mouseY, layout.controlsResetAllButton);
        rc2d_graphics_setColor(resetAllHovered ? kButtonFillHover : kButtonFillMuted);
        rc2d_graphics_rectangle("fill", &layout.controlsResetAllButton);
        rc2d_graphics_setColor(resetAllHovered ? kSliderThumbBorder : kGold);
        rc2d_graphics_rectangle("line", &layout.controlsResetAllButton);
        drawCentered(&self->smallFont, "Réinitialiser tout", layout.controlsResetAllButton, kTextGold);
    }
    else if (self->activeTab == SettingsTab::GRAPHICS)
    {
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.graphicsPanel);

        self->graphicsScrollOffsetY =
            clampf(self->graphicsScrollOffsetY, 0.0f, getGraphicsMaxScrollOffset(layout));
        const float scrollY = self->graphicsScrollOffsetY;
        SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
        if (renderer != nullptr)
        {
            const SDL_Rect clipRect = toClipRect(layout.graphicsViewport);
            SDL_SetRenderClipRect(renderer, &clipRect);
        }

        const SDL_FRect graphicsGeneralPanel = offsetRect(layout.graphicsGeneralPanel, 0.0f, -scrollY);
        const SDL_FRect graphicsVsyncRow = offsetRect(layout.graphicsVsyncRow, 0.0f, -scrollY);
        const SDL_FRect graphicsVsyncCheckbox = offsetRect(layout.graphicsVsyncCheckbox, 0.0f, -scrollY);
        const SDL_FRect graphicsWindowPanel = offsetRect(layout.graphicsWindowPanel, 0.0f, -scrollY);
        const SDL_FRect graphicsWindowModeRow = offsetRect(layout.graphicsWindowModeRow, 0.0f, -scrollY);
        const SDL_FRect graphicsWindowModeButton = offsetRect(layout.graphicsWindowModeButton, 0.0f, -scrollY);
        const SDL_FRect graphicsWindowModeDropdown = offsetRect(layout.graphicsWindowModeDropdown, 0.0f, -scrollY);
        const SDL_FRect graphicsPresentationModeRow = offsetRect(layout.graphicsPresentationModeRow, 0.0f, -scrollY);
        const SDL_FRect graphicsPresentationModeButton = offsetRect(layout.graphicsPresentationModeButton, 0.0f, -scrollY);
        const SDL_FRect graphicsPresentationModeDropdown = offsetRect(layout.graphicsPresentationModeDropdown, 0.0f, -scrollY);
        const SDL_FRect graphicsMonitorFpsRow = offsetRect(layout.graphicsMonitorFpsRow, 0.0f, -scrollY);
        const SDL_FRect graphicsMonitorFpsBox = offsetRect(layout.graphicsMonitorFpsBox, 0.0f, -scrollY);
        const SDL_FRect graphicsCursorLockRow = offsetRect(layout.graphicsCursorLockRow, 0.0f, -scrollY);
        const SDL_FRect graphicsCursorLockCheckbox = offsetRect(layout.graphicsCursorLockCheckbox, 0.0f, -scrollY);
        const SDL_FRect graphicsInterfacePanel = offsetRect(layout.graphicsInterfacePanel, 0.0f, -scrollY);
        const SDL_FRect graphicsHideCoordinateBackgroundRow =
            offsetRect(layout.graphicsHideCoordinateBackgroundRow, 0.0f, -scrollY);
        const SDL_FRect graphicsHideCoordinateBackgroundCheckbox =
            offsetRect(layout.graphicsHideCoordinateBackgroundCheckbox, 0.0f, -scrollY);
        const SDL_FRect graphicsAnimationsPanel = offsetRect(layout.graphicsAnimationsPanel, 0.0f, -scrollY);
        const SDL_FRect graphicsFogOfWarRow = offsetRect(layout.graphicsFogOfWarRow, 0.0f, -scrollY);
        const SDL_FRect graphicsFogOfWarCheckbox = offsetRect(layout.graphicsFogOfWarCheckbox, 0.0f, -scrollY);
        const SDL_FRect graphicsShipWakeTrailsRow = offsetRect(layout.graphicsShipWakeTrailsRow, 0.0f, -scrollY);
        const SDL_FRect graphicsShipWakeTrailsCheckbox =
            offsetRect(layout.graphicsShipWakeTrailsCheckbox, 0.0f, -scrollY);
        const SDL_FRect graphicsHideOtherPlayersVfxRow = offsetRect(layout.graphicsHideOtherPlayersVfxRow, 0.0f, -scrollY);
        const SDL_FRect graphicsHideOtherPlayersVfxCheckbox =
            offsetRect(layout.graphicsHideOtherPlayersVfxCheckbox, 0.0f, -scrollY);
        const SDL_FRect graphicsGameplayPanel = offsetRect(layout.graphicsGameplayPanel, 0.0f, -scrollY);
        const SDL_FRect graphicsSalvoRow = offsetRect(layout.graphicsSalvoRow, 0.0f, -scrollY);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &graphicsGeneralPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &graphicsGeneralPanel);
        drawTextAt(
            &self->titleFont,
            "Général",
            graphicsGeneralPanel.x + 16.0f,
            graphicsGeneralPanel.y + 14.0f,
            kTextGold);

        drawTextAt(
            &self->bodyFont,
            "Synchronisation Verticale",
            graphicsVsyncRow.x,
            graphicsVsyncRow.y + 3.0f,
            kTextBody);
        drawTextAt(
            &self->bodyFont,
            "(VSync)",
            graphicsVsyncRow.x,
            graphicsVsyncRow.y + 22.0f,
            kTextBody);
        drawCheckBox(graphicsVsyncCheckbox, rc2d_window_getVSync(), self->checkboxValidIcon);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &graphicsWindowPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &graphicsWindowPanel);
        drawTextAt(
            &self->titleFont,
            "Fenêtre",
            graphicsWindowPanel.x + 16.0f,
            graphicsWindowPanel.y + 14.0f,
            kTextGold);

        drawLeftCenteredY(
            &self->bodyFont,
            "Mode fenêtre",
            graphicsWindowModeRow,
            graphicsWindowModeRow.x,
            kTextBody);

        const bool windowModeHovered = isPointInRect(mouseX, mouseY, graphicsWindowModeButton);
        rc2d_graphics_setColor(windowModeHovered ? kButtonFillHover : kFieldFill);
        rc2d_graphics_rectangle("fill", &graphicsWindowModeButton);
        rc2d_graphics_setColor(windowModeHovered ? kSliderThumbBorder : kGold);
        rc2d_graphics_rectangle("line", &graphicsWindowModeButton);
        drawLeftCenteredY(
            &self->bodyFont,
            getGraphicsWindowModeLabel(self->getGraphicsWindowMode()),
            graphicsWindowModeButton,
            graphicsWindowModeButton.x + 10.0f,
            kTextGold);

        const SDL_FRect windowModeArrowRect = SDL_FRect{
            graphicsWindowModeButton.x + graphicsWindowModeButton.w - 22.0f,
            graphicsWindowModeButton.y + ((graphicsWindowModeButton.h - 14.0f) * 0.5f),
            14.0f,
            14.0f
        };
        drawImageFit(self->arrowDownIcon, windowModeArrowRect, 0.0f);

        drawLeftCenteredY(
            &self->bodyFont,
            "Mode d'affichage",
            graphicsPresentationModeRow,
            graphicsPresentationModeRow.x,
            kTextBody);

        const bool presentationModeHovered = isPointInRect(mouseX, mouseY, graphicsPresentationModeButton);
        rc2d_graphics_setColor(presentationModeHovered ? kButtonFillHover : kFieldFill);
        rc2d_graphics_rectangle("fill", &graphicsPresentationModeButton);
        rc2d_graphics_setColor(presentationModeHovered ? kSliderThumbBorder : kGold);
        rc2d_graphics_rectangle("line", &graphicsPresentationModeButton);
        drawLeftCenteredY(
            &self->bodyFont,
            getGraphicsPresentationModeLabel(self->getGraphicsPresentationMode()),
            graphicsPresentationModeButton,
            graphicsPresentationModeButton.x + 10.0f,
            kTextGold);

        const SDL_FRect presentationModeArrowRect = SDL_FRect{
            graphicsPresentationModeButton.x + graphicsPresentationModeButton.w - 22.0f,
            graphicsPresentationModeButton.y + ((graphicsPresentationModeButton.h - 14.0f) * 0.5f),
            14.0f,
            14.0f
        };
        drawImageFit(self->arrowDownIcon, presentationModeArrowRect, 0.0f);

        drawLeftCenteredY(
            &self->bodyFont,
            "FPS moniteur",
            graphicsMonitorFpsRow,
            graphicsMonitorFpsRow.x,
            kTextBody);

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &graphicsMonitorFpsBox);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &graphicsMonitorFpsBox);

        drawLeftCenteredY(
            &self->bodyFont,
            getCurrentMonitorRefreshRateLabel(),
            graphicsMonitorFpsBox,
            graphicsMonitorFpsBox.x + 10.0f,
            kTextGold);

        drawLeftCenteredY(
            &self->bodyFont,
            "Verrouiller le curseur dans la fenêtre",
            graphicsCursorLockRow,
            graphicsCursorLockRow.x,
            kTextBody);
        drawCheckBox(
            graphicsCursorLockCheckbox,
            rc2d_window_isMouseGrabbed(),
            self->checkboxValidIcon);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &graphicsInterfacePanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &graphicsInterfacePanel);
        drawTextAt(
            &self->titleFont,
            "Interface utilisateur",
            graphicsInterfacePanel.x + 16.0f,
            graphicsInterfacePanel.y + 14.0f,
            kTextGold);

        drawLeftCenteredY(
            &self->bodyFont,
            "Masquer l'arrière-plan des coordonnées",
            graphicsHideCoordinateBackgroundRow,
            graphicsHideCoordinateBackgroundRow.x,
            kTextBody);
        drawCheckBox(
            graphicsHideCoordinateBackgroundCheckbox,
            self->graphicsHideCoordinateBackground,
            self->checkboxValidIcon);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &graphicsAnimationsPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &graphicsAnimationsPanel);
        drawTextAt(
            &self->titleFont,
            "Animations",
            graphicsAnimationsPanel.x + 16.0f,
            graphicsAnimationsPanel.y + 14.0f,
            kTextGold);

        drawLeftCenteredY(
            &self->bodyFont,
            "Brouillard de guerre",
            graphicsFogOfWarRow,
            graphicsFogOfWarRow.x,
            kTextBody);
        drawCheckBox(
            graphicsFogOfWarCheckbox,
            g_graphicsFogOfWarEnabled,
            self->checkboxValidIcon);

        drawLeftCenteredY(
            &self->bodyFont,
            "Trainée des navires sur l'ocean",
            graphicsShipWakeTrailsRow,
            graphicsShipWakeTrailsRow.x,
            kTextBody);
        drawCheckBox(
            graphicsShipWakeTrailsCheckbox,
            g_graphicsShipWakeTrailsEnabled,
            self->checkboxValidIcon);

        drawLeftCenteredY(
            &self->bodyFont,
            "Désactiver les VFX des autres joueurs (n'affiche plus les boulets, impacts, effets de speed, etc.)",
            graphicsHideOtherPlayersVfxRow,
            graphicsHideOtherPlayersVfxRow.x,
            kTextBody);
        drawCheckBox(
            graphicsHideOtherPlayersVfxCheckbox,
            self->graphicsHideOtherPlayersVfx,
            self->checkboxValidIcon);

        if (self->graphicsWindowModeDropdownOpen)
        {
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &graphicsWindowModeDropdown);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &graphicsWindowModeDropdown);

            const GraphicsWindowMode currentMode = self->getGraphicsWindowMode();
            for (std::size_t index = 0; index < layout.graphicsWindowModeOptions.size(); ++index)
            {
                const GraphicsWindowMode mode = getGraphicsWindowModeForOptionIndex(index);
                const bool selected = mode == currentMode;
                const SDL_FRect optionRect = offsetRect(layout.graphicsWindowModeOptions[index], 0.0f, -scrollY);
                const bool hovered = isPointInRect(mouseX, mouseY, optionRect);
                if (selected || hovered)
                {
                    rc2d_graphics_setColor(selected ? kTabActive : kSelectionFill);
                    rc2d_graphics_rectangle("fill", &optionRect);
                }

                rc2d_graphics_setColor(kRowLine);
                rc2d_graphics_rectangle("line", &optionRect);
                drawLeftCenteredY(
                    &self->bodyFont,
                    getGraphicsWindowModeLabel(mode),
                    optionRect,
                    optionRect.x + 10.0f,
                    selected ? kTextGold : kTextBody);
            }
        }

        if (self->graphicsPresentationModeDropdownOpen)
        {
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &graphicsPresentationModeDropdown);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &graphicsPresentationModeDropdown);

            const RC2D_LogicalPresentationMode currentMode = self->getGraphicsPresentationMode();
            for (std::size_t index = 0; index < layout.graphicsPresentationModeOptions.size(); ++index)
            {
                const RC2D_LogicalPresentationMode mode =
                    index == 0U
                        ? RC2D_LOGICAL_PRESENTATION_OVERSCAN
                        : RC2D_LOGICAL_PRESENTATION_LETTERBOX;
                const bool selected = mode == currentMode;
                const SDL_FRect optionRect = offsetRect(layout.graphicsPresentationModeOptions[index], 0.0f, -scrollY);
                const bool hovered = isPointInRect(mouseX, mouseY, optionRect);
                if (selected || hovered)
                {
                    rc2d_graphics_setColor(selected ? kTabActive : kSelectionFill);
                    rc2d_graphics_rectangle("fill", &optionRect);
                }

                rc2d_graphics_setColor(kRowLine);
                rc2d_graphics_rectangle("line", &optionRect);
                drawLeftCenteredY(
                    &self->bodyFont,
                    getGraphicsPresentationModeLabel(mode),
                    optionRect,
                    optionRect.x + 10.0f,
                    selected ? kTextGold : kTextBody);
            }
        }

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &graphicsGameplayPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &graphicsGameplayPanel);
        drawTextAt(
            &self->titleFont,
            "Gameplay",
            graphicsGameplayPanel.x + 16.0f,
            graphicsGameplayPanel.y + 14.0f,
            kTextGold);

        drawLeftCenteredY(
            &self->bodyFont,
            "Nombre de boulets par salve",
            graphicsSalvoRow,
            graphicsSalvoRow.x,
            kTextBody);

        const std::array<const char*, 3> salvoLabels = {"Low (1)", "Normal (5)", "High (10)"};
        for (std::size_t index = 0; index < layout.graphicsSalvoOptions.size(); ++index)
        {
            const SDL_FRect optionRect = offsetRect(layout.graphicsSalvoOptions[index], 0.0f, -scrollY);
            const bool selected = static_cast<int>(self->graphicsSalvoBulletPreset) == static_cast<int>(index);
            const bool hovered = isPointInRect(mouseX, mouseY, optionRect);
            rc2d_graphics_setColor(selected ? kButtonFillActive : (hovered ? kButtonFillHover : kFieldFill));
            rc2d_graphics_rectangle("fill", &optionRect);
            rc2d_graphics_setColor(selected ? kSliderThumbBorder : kGold);
            rc2d_graphics_rectangle("line", &optionRect);
            drawCentered(&self->smallFont, salvoLabels[index], optionRect, selected ? kTextGold : kTextBody);
        }

        if (renderer != nullptr)
        {
            SDL_SetRenderClipRect(renderer, nullptr);
        }

        const float maxGraphicsScrollOffset = getGraphicsMaxScrollOffset(layout);
        if (maxGraphicsScrollOffset > 0.0f)
        {
            const SDL_FRect thumbRect = getGraphicsScrollThumbRect(layout, self->graphicsScrollOffsetY);
            rc2d_graphics_setColor(kScrollTrack);
            rc2d_graphics_rectangle("fill", &layout.graphicsScrollTrack);
            rc2d_graphics_setColor(kRowLine);
            rc2d_graphics_rectangle("line", &layout.graphicsScrollTrack);
            rc2d_graphics_setColor((self->graphicsScrollDragging || (self->graphicsScrollWheelHighlightSec > 0.0f)) ? kSliderThumbFill : kScrollThumb);
            rc2d_graphics_rectangle("fill", &thumbRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &thumbRect);
        }
    }
    else
    {
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.placeholderPanel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.placeholderPanel);

        const char* title = "Sons";
        const char* description = "Le contenu de l'onglet sons sera ajoute prochainement.";

        drawTextAt(&self->titleFont, title, layout.placeholderPanel.x + 20.0f, layout.placeholderPanel.y + 22.0f, kTextGold);
        drawWrappedText(
            &self->bodyFont,
            description,
            SDL_FRect{
                layout.placeholderPanel.x + 20.0f,
                layout.placeholderPanel.y + 66.0f,
                layout.placeholderPanel.w - 40.0f,
                80.0f
            },
            kTextBody,
            4.0f);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

bool GameSettingsWidget::containsPoint(float x, float y) const
{
    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    return isPointInRect(x, y, currentRect);
}

HudCursorType GameSettingsWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }

    if (this->hudScaleDragging)
    {
        return HudCursorType::RESIZE_HORIZONTAL;
    }
    if (this->mapFrameMarginDragging)
    {
        return HudCursorType::RESIZE_HORIZONTAL;
    }
    if (this->cameraScrollSpeedDragging)
    {
        return HudCursorType::RESIZE_HORIZONTAL;
    }
    if (this->controlsScrollDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->graphicsScrollDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->languageScrollDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }
    if (!this->containsPoint(x, y))
    {
        return HudCursorType::NONE;
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const GameSettingsLayout layout = buildLayout(currentRect, static_cast<int>(this->languageOptions.size()));

    if (isPointInRect(x, y, layout.closeButton))
    {
        return HudCursorType::POINTER;
    }
    for (const SDL_FRect& tabRect : layout.tabs)
    {
        if (isPointInRect(x, y, tabRect))
        {
            return HudCursorType::POINTER;
        }
    }

    if (this->activeTab == SettingsTab::GENERAL)
    {
        if (this->languageDropdownOpen)
        {
            const int visibleRows = this->getVisibleLanguageRowCount();
            const int maxFirstRow = getLanguageMaxFirstRow(static_cast<int>(this->languageOptions.size()), visibleRows);
            if (maxFirstRow > 0 && isPointInRect(x, y, layout.languageScrollTrack))
            {
                return HudCursorType::RESIZE_VERTICAL;
            }

            if (isPointInRect(x, y, layout.languageViewport))
            {
                for (int row = 0; row < visibleRows; ++row)
                {
                    const int optionIndex = this->languageFirstRow + row;
                    if (optionIndex < 0 || optionIndex >= static_cast<int>(this->languageOptions.size()))
                    {
                        break;
                    }

                    const SDL_FRect rowRect = SDL_FRect{
                        layout.languageViewport.x,
                        layout.languageViewport.y + (static_cast<float>(row) * kLanguageRowHeight),
                        layout.languageViewport.w,
                        kLanguageRowHeight
                    };
                    if (isPointInRect(x, y, rowRect))
                    {
                        return HudCursorType::POINTER;
                    }
                }
            }
        }

        if (isPointInRect(x, y, layout.redeemInput))
        {
            return HudCursorType::TEXT;
        }

        for (std::size_t index = 0; index < layout.hudScaleTracks.size(); ++index)
        {
            const SDL_FRect& trackRect = layout.hudScaleTracks[index];
            const HudScaleTarget target = static_cast<HudScaleTarget>(index);
            const SDL_FRect thumbRect = getHudScaleThumbRect(trackRect, target, this->hudScaleValues[index]);
            if (isPointInRect(x, y, trackRect) || isPointInRect(x, y, thumbRect))
            {
                return HudCursorType::RESIZE_HORIZONTAL;
            }
        }
        for (std::size_t index = 0; index < layout.mapFrameTracks.size(); ++index)
        {
            const SDL_FRect& trackRect = layout.mapFrameTracks[index];
            const SDL_FRect thumbRect =
                getMapFrameMarginThumbRect(trackRect, this->mapPlayfieldFrameMarginPercent[index]);
            if (isPointInRect(x, y, trackRect) || isPointInRect(x, y, thumbRect))
            {
                return HudCursorType::RESIZE_HORIZONTAL;
            }
        }
        for (const SDL_FRect& buttonRect : layout.hudScaleVisibilityButtons)
        {
            if (isPointInRect(x, y, buttonRect))
            {
                return HudCursorType::POINTER;
            }
        }
        if (isPointInRect(x, y, layout.redeemButton) ||
            isPointInRect(x, y, layout.configuratorButton) ||
            isPointInRect(x, y, layout.languageButton))
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == SettingsTab::CONTROLS)
    {
        const float maxControlsScrollOffset = getControlsMaxScrollOffset(layout);
        if (maxControlsScrollOffset > 0.0f && isPointInRect(x, y, layout.controlsScrollTrack))
        {
            return HudCursorType::RESIZE_VERTICAL;
        }
        if (isPointInRect(x, y, layout.controlsResetAllButton))
        {
            return HudCursorType::POINTER;
        }
        if (isPointInRect(x, y, layout.controlsViewport))
        {
            float entryY = layout.controlsViewport.y - this->controlsScrollOffsetY;
            for (const GameSettingsControlsEntry& entry : kControlsEntries)
            {
                const float entryHeight = getControlsEntryHeight(entry.type);
                const SDL_FRect rowRect = SDL_FRect{
                    layout.controlsViewport.x,
                    entryY,
                    layout.controlsViewport.w,
                    entryHeight
                };
                if (isPointInRect(x, y, rowRect))
                {
                    if (entry.type == ControlsEntryType::ACTION)
                    {
                        const SDL_FRect resetButtonRect = SDL_FRect{
                            rowRect.x + rowRect.w - kControlsResetButtonWidth,
                            rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                            kControlsResetButtonWidth,
                            kControlsButtonHeight
                        };
                        const SDL_FRect keyButtonRect = SDL_FRect{
                            resetButtonRect.x - 10.0f - kControlsKeyButtonWidth,
                            rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                            kControlsKeyButtonWidth,
                            kControlsButtonHeight
                        };
                        if (isPointInRect(x, y, resetButtonRect) || isPointInRect(x, y, keyButtonRect))
                        {
                            return HudCursorType::POINTER;
                        }
                    }
                    else if (entry.type == ControlsEntryType::CAMERA_SPEED)
                    {
                        const SDL_FRect resetButtonRect = SDL_FRect{
                            rowRect.x + rowRect.w - kControlsResetButtonWidth,
                            rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                            kControlsResetButtonWidth,
                            kControlsButtonHeight
                        };
                        const SDL_FRect trackRect = SDL_FRect{
                            rowRect.x + 292.0f,
                            rowRect.y + ((rowRect.h - kControlsTrackHeight) * 0.5f),
                            (std::max)(1.0f, resetButtonRect.x - 70.0f - (rowRect.x + 292.0f)),
                            kControlsTrackHeight
                        };
                        const SDL_FRect thumbRect =
                            getCameraScrollSpeedThumbRect(trackRect, this->cameraScrollSpeedSectors);
                        if (isPointInRect(x, y, resetButtonRect))
                        {
                            return HudCursorType::POINTER;
                        }
                        if (isPointInRect(x, y, trackRect) || isPointInRect(x, y, thumbRect))
                        {
                            return HudCursorType::RESIZE_HORIZONTAL;
                        }
                    }
                    break;
                }

                entryY += entryHeight + kControlsRowGap;
            }
        }
    }
    else if (this->activeTab == SettingsTab::GRAPHICS)
    {
        const float maxGraphicsScrollOffset = getGraphicsMaxScrollOffset(layout);
        if (maxGraphicsScrollOffset > 0.0f && isPointInRect(x, y, layout.graphicsScrollTrack))
        {
            return HudCursorType::RESIZE_VERTICAL;
        }

        const float scrollY = this->graphicsScrollOffsetY;
        const SDL_FRect graphicsWindowModeButton = offsetRect(layout.graphicsWindowModeButton, 0.0f, -scrollY);
        const SDL_FRect graphicsPresentationModeButton = offsetRect(layout.graphicsPresentationModeButton, 0.0f, -scrollY);
        const SDL_FRect graphicsVsyncRow = offsetRect(layout.graphicsVsyncRow, 0.0f, -scrollY);
        const SDL_FRect graphicsCursorLockRow = offsetRect(layout.graphicsCursorLockRow, 0.0f, -scrollY);
        const SDL_FRect graphicsHideCoordinateBackgroundRow = offsetRect(layout.graphicsHideCoordinateBackgroundRow, 0.0f, -scrollY);
        const SDL_FRect graphicsFogOfWarRow = offsetRect(layout.graphicsFogOfWarRow, 0.0f, -scrollY);
        const SDL_FRect graphicsShipWakeTrailsRow = offsetRect(layout.graphicsShipWakeTrailsRow, 0.0f, -scrollY);
        const SDL_FRect graphicsHideOtherPlayersVfxRow = offsetRect(layout.graphicsHideOtherPlayersVfxRow, 0.0f, -scrollY);

        if (this->graphicsWindowModeDropdownOpen)
        {
            for (const SDL_FRect& optionRect : layout.graphicsWindowModeOptions)
            {
                if (isPointInRect(x, y, offsetRect(optionRect, 0.0f, -scrollY)))
                {
                    return HudCursorType::POINTER;
                }
            }
        }
        if (this->graphicsPresentationModeDropdownOpen)
        {
            for (const SDL_FRect& optionRect : layout.graphicsPresentationModeOptions)
            {
                if (isPointInRect(x, y, offsetRect(optionRect, 0.0f, -scrollY)))
                {
                    return HudCursorType::POINTER;
                }
            }
        }

        for (const SDL_FRect& optionRect : layout.graphicsSalvoOptions)
        {
            if (isPointInRect(x, y, offsetRect(optionRect, 0.0f, -scrollY)))
            {
                return HudCursorType::POINTER;
            }
        }

        if (isPointInRect(x, y, graphicsVsyncRow) ||
            isPointInRect(x, y, graphicsCursorLockRow) ||
            isPointInRect(x, y, graphicsHideCoordinateBackgroundRow) ||
            isPointInRect(x, y, graphicsFogOfWarRow) ||
            isPointInRect(x, y, graphicsShipWakeTrailsRow) ||
            isPointInRect(x, y, graphicsHideOtherPlayersVfxRow) ||
            isPointInRect(x, y, graphicsWindowModeButton) ||
            isPointInRect(x, y, graphicsPresentationModeButton))
        {
            return HudCursorType::POINTER;
        }
    }

    if (isPointInRect(x, y, layout.headerDragRect))
    {
        return HudCursorType::MOVE;
    }
    return HudCursorType::DEFAULT;
}

void GameSettingsWidget::clearFocus(void)
{
    this->redeemInputFocused = false;
    this->redeemInputSelectingWithMouse = false;
    this->redeemCursorVisible = false;
    this->redeemCursorBlinkElapsed = 0.0;
    this->clearRedeemSelection();
    this->redeemEditHistory.clear();
    this->languageDropdownOpen = false;
    this->languageScrollDragging = false;
    this->languageScrollWheelHighlightSec = 0.0f;
    this->hudScaleDragging = false;
    this->mapFrameMarginDragging = false;
    this->controlsScrollDragging = false;
    this->cameraScrollSpeedDragging = false;
    this->graphicsScrollDragging = false;
    this->controlCaptureActive = false;
    this->controlConflictPending = false;
    this->graphicsWindowModeDropdownOpen = false;
    this->graphicsPresentationModeDropdownOpen = false;
    this->hoveredLanguageIndex = -1;
}

void GameSettingsWidget::show(void)
{
    this->visible = true;
    this->activeTab = SettingsTab::GENERAL;
    this->widgetDragging = false;
    this->languageScrollDragging = false;
    this->hudScaleDragging = false;
    this->mapFrameMarginDragging = false;
    this->controlsScrollDragging = false;
    this->cameraScrollSpeedDragging = false;
    this->graphicsScrollDragging = false;
    this->controlCaptureActive = false;
    this->controlConflictPending = false;
    this->graphicsWindowModeDropdownOpen = false;
    this->graphicsPresentationModeDropdownOpen = false;
    this->syncSelectedLanguageFromContext();
    this->scrollLanguageToSelection();
    this->clampControlsScroll();
    this->clampGraphicsScroll();
}

void GameSettingsWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->languageScrollDragging = false;
    this->hudScaleDragging = false;
    this->mapFrameMarginDragging = false;
    this->controlsScrollDragging = false;
    this->cameraScrollSpeedDragging = false;
    this->graphicsScrollDragging = false;
    this->languageScrollWheelHighlightSec = 0.0f;
    this->controlsScrollWheelHighlightSec = 0.0f;
    this->graphicsScrollWheelHighlightSec = 0.0f;
    this->controlCaptureActive = false;
    this->graphicsWindowModeDropdownOpen = false;
    this->graphicsPresentationModeDropdownOpen = false;
    this->clearFocus();
}

void GameSettingsWidget::setRedeemCode(const std::string& value)
{
    this->redeemCode = sanitizeRedeemCodeValue(value);
    this->redeemCursorIndex = this->redeemCode.size();
    this->clearRedeemSelection();
    this->redeemEditHistory.clear();
}

const std::string& GameSettingsWidget::getRedeemCode(void) const
{
    return this->redeemCode;
}

void GameSettingsWidget::setOnRedeemCodeRequested(RedeemCodeCallback callback)
{
    this->onRedeemCodeRequested = std::move(callback);
}

void GameSettingsWidget::setOnStartUiConfiguratorRequested(VoidCallback callback)
{
    this->onStartUiConfiguratorRequested = std::move(callback);
}

void GameSettingsWidget::setOnLanguageChanged(LanguageChangedCallback callback)
{
    this->onLanguageChanged = std::move(callback);
}

void GameSettingsWidget::setHudScaleValue(HudScaleTarget target, float scale)
{
    this->applyHudScaleValue(target, scale, false);
}

float GameSettingsWidget::getHudScaleValue(HudScaleTarget target) const
{
    return this->hudScaleValues[static_cast<std::size_t>(target)];
}

void GameSettingsWidget::setOnHudScaleChanged(HudScaleChangedCallback callback)
{
    this->onHudScaleChanged = std::move(callback);
}

void GameSettingsWidget::setHudVisibilityValue(HudScaleTarget target, bool isVisible)
{
    this->applyHudVisibilityValue(target, isVisible, false);
}

bool GameSettingsWidget::getHudVisibilityValue(HudScaleTarget target) const
{
    return this->hudVisibilityValues[static_cast<std::size_t>(target)];
}

void GameSettingsWidget::setOnHudVisibilityChanged(HudVisibilityChangedCallback callback)
{
    this->onHudVisibilityChanged = std::move(callback);
}

SDL_Scancode GameSettingsWidget::getControlActionScancode(ControlAction action) const
{
    const std::size_t index = static_cast<std::size_t>(action);
    if (index >= this->controlActionScancodes.size())
    {
        return SDL_SCANCODE_UNKNOWN;
    }

    return this->controlActionScancodes[index];
}

SDL_Keycode GameSettingsWidget::getControlActionKeycode(ControlAction action) const
{
    const SDL_Scancode scancode = this->getControlActionScancode(action);
    if (scancode == SDL_SCANCODE_UNKNOWN)
    {
        return SDLK_UNKNOWN;
    }

    return SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, true);
}

std::string GameSettingsWidget::getControlActionLabel(ControlAction action) const
{
    return getDisplayNameForScancode(this->getControlActionScancode(action));
}

float GameSettingsWidget::getCameraScrollSpeedSectors(void) const
{
    return this->cameraScrollSpeedSectors;
}

void GameSettingsWidget::setCameraScrollSpeedSectors(float speedSectors)
{
    this->cameraScrollSpeedSectors = snapCameraScrollSpeed(speedSectors);
    if (!this->cameraScrollSpeedDragging && this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

bool GameSettingsWidget::getHideCoordinateBackground(void) const
{
    return this->graphicsHideCoordinateBackground;
}

bool GameSettingsWidget::getFogOfWarEnabled(void) const
{
    return g_graphicsFogOfWarEnabled;
}

bool GameSettingsWidget::getShipWakeTrailsEnabled(void) const
{
    return g_graphicsShipWakeTrailsEnabled;
}

bool GameSettingsWidget::getHideOtherPlayersVfxEnabled(void) const
{
    return this->graphicsHideOtherPlayersVfx;
}

void GameSettingsWidget::setHideOtherPlayersVfxEnabled(bool enabled)
{
    this->graphicsHideOtherPlayersVfx = enabled;
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

GameSettingsWidget::SalvoBulletPreset GameSettingsWidget::getSalvoBulletPreset(void) const
{
    return this->graphicsSalvoBulletPreset;
}

void GameSettingsWidget::setSalvoBulletPreset(SalvoBulletPreset preset)
{
    this->graphicsSalvoBulletPreset = preset;
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

int GameSettingsWidget::getSalvoBulletCount(void) const
{
    switch (this->graphicsSalvoBulletPreset)
    {
    case SalvoBulletPreset::LOW:
        return 1;
    case SalvoBulletPreset::NORMAL:
        return 5;
    case SalvoBulletPreset::HIGH:
        return 10;
    default:
        return 5;
    }
}

MapPlayfieldFrameMarginsPercent GameSettingsWidget::getMapPlayfieldFrameMarginsPercent(void) const
{
    MapPlayfieldFrameMarginsPercent margins{};
    margins.left = this->mapPlayfieldFrameMarginPercent[0];
    margins.right = this->mapPlayfieldFrameMarginPercent[1];
    margins.bottom = this->mapPlayfieldFrameMarginPercent[2];
    return margins;
}

void GameSettingsWidget::setMapPlayfieldFrameMarginsPercent(
    const MapPlayfieldFrameMarginsPercent& margins,
    bool notifyUserSettingsChanged)
{
    this->mapPlayfieldFrameMarginPercent[0] = margins.left;
    this->mapPlayfieldFrameMarginPercent[1] = margins.right;
    this->mapPlayfieldFrameMarginPercent[2] = margins.bottom;
    this->applyMapPlayfieldFrameMargins(notifyUserSettingsChanged);
}

void GameSettingsWidget::applyMapPlayfieldFrameMargins(bool notifyUserSettingsChanged)
{
    MapPlayfieldFrameMarginsPercent request{};
    request.left = this->mapPlayfieldFrameMarginPercent[0];
    request.right = this->mapPlayfieldFrameMarginPercent[1];
    request.bottom = this->mapPlayfieldFrameMarginPercent[2];
    MapSetPlayfieldFrameMarginsPercent(request);
    const MapPlayfieldFrameMarginsPercent applied = MapGetPlayfieldFrameMarginsPercent();
    this->mapPlayfieldFrameMarginPercent[0] = applied.left;
    this->mapPlayfieldFrameMarginPercent[1] = applied.right;
    this->mapPlayfieldFrameMarginPercent[2] = applied.bottom;

    if (notifyUserSettingsChanged && this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

void GameSettingsWidget::updateDraggedMapFrameMarginFromMouse(float mouseX)
{
    const std::size_t index = this->mapFrameMarginDragRow;
    if (index >= this->mapPlayfieldFrameMarginPercent.size())
    {
        return;
    }

    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    const SDL_FRect& trackRect = layout.mapFrameTracks[index];
    if (trackRect.w <= 0.0f)
    {
        return;
    }

    const float thumbLeft = clampf(
        mouseX - this->mapFrameMarginDragGrabOffsetX,
        trackRect.x - (kHudScaleThumbWidth * 0.5f),
        trackRect.x + trackRect.w - (kHudScaleThumbWidth * 0.5f));
    const float thumbCenterX = thumbLeft + (kHudScaleThumbWidth * 0.5f);
    const float normalized = (thumbCenterX - trackRect.x) / trackRect.w;
    const int pct = normalizedToMapFrameMarginPct(normalized);
    if (this->mapPlayfieldFrameMarginPercent[index] != pct)
    {
        this->mapPlayfieldFrameMarginPercent[index] = pct;
        this->applyMapPlayfieldFrameMargins(false);
    }
}

void GameSettingsWidget::setHideCoordinateBackground(bool value)
{
    this->graphicsHideCoordinateBackground = value;
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

void GameSettingsWidget::setFogOfWarEnabled(bool value)
{
    g_graphicsFogOfWarEnabled = value;
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

void GameSettingsWidget::setShipWakeTrailsEnabled(bool value)
{
    g_graphicsShipWakeTrailsEnabled = value;
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

void GameSettingsWidget::setControlActionScancode(ControlAction action, SDL_Scancode scancode)
{
    this->applyControlActionScancode(action, scancode);
}

void GameSettingsWidget::setOnUserSettingsChanged(VoidCallback callback)
{
    this->onUserSettingsChanged = std::move(callback);
}

void GameSettingsWidget::rebuildLanguageOptions(void)
{
    this->languageOptions.clear();
    this->languageOptions.reserve(kLanguageCatalog.size());
    for (const GameSettingsLanguageCatalogEntry& entry : kLanguageCatalog)
    {
        this->languageOptions.push_back(LanguageOption{entry.flagName, entry.label});
    }
}

void GameSettingsWidget::syncSelectedLanguageFromContext(void)
{
    const ClientLanguageState& languageState = GetClientLanguageState();

    std::string wantedFlagName = languageState.getCurrentFlagName();
    if (wantedFlagName.empty())
    {
        wantedFlagName = languageState.getDetectedFlagName();
    }
    if (wantedFlagName.empty())
    {
        wantedFlagName = "england";
    }

    this->selectedLanguageIndex = -1;
    for (std::size_t index = 0; index < this->languageOptions.size(); ++index)
    {
        if (this->languageOptions[index].flagName == wantedFlagName)
        {
            this->selectedLanguageIndex = static_cast<int>(index);
            return;
        }
    }

    if (!this->languageOptions.empty())
    {
        this->selectedLanguageIndex = 0;
    }
}

void GameSettingsWidget::applySelectedLanguageToContext(void)
{
    if (this->selectedLanguageIndex < 0 ||
        this->selectedLanguageIndex >= static_cast<int>(this->languageOptions.size()))
    {
        return;
    }

    const std::string& selectedFlagName =
        this->languageOptions[static_cast<std::size_t>(this->selectedLanguageIndex)].flagName;
    const std::string previousFlagName = GetClientLanguageState().getCurrentFlagName();
    GetClientLanguageState().setCurrentFlagName(selectedFlagName);
    if (this->onLanguageChanged && previousFlagName != selectedFlagName)
    {
        this->onLanguageChanged(selectedFlagName);
    }
}

void GameSettingsWidget::clampLanguageScroll(void)
{
    const int visibleRows = this->getVisibleLanguageRowCount();
    const int maxFirstRow = getLanguageMaxFirstRow(static_cast<int>(this->languageOptions.size()), visibleRows);
    this->languageFirstRow = (std::max)(0, (std::min)(this->languageFirstRow, maxFirstRow));
}

void GameSettingsWidget::scrollLanguageToSelection(void)
{
    if (this->selectedLanguageIndex < 0)
    {
        this->languageFirstRow = 0;
        return;
    }

    const int visibleRows = this->getVisibleLanguageRowCount();
    if (this->selectedLanguageIndex < this->languageFirstRow)
    {
        this->languageFirstRow = this->selectedLanguageIndex;
    }
    else if (this->selectedLanguageIndex >= this->languageFirstRow + visibleRows)
    {
        this->languageFirstRow = this->selectedLanguageIndex - visibleRows + 1;
    }
    this->clampLanguageScroll();
}

int GameSettingsWidget::getVisibleLanguageRowCount(void) const
{
    return getVisibleLanguageRowCountForTotal(static_cast<int>(this->languageOptions.size()));
}

void GameSettingsWidget::triggerRedeemCodeRequest(void)
{
    if (!this->onRedeemCodeRequested || this->redeemCode.empty())
    {
        return;
    }

    this->onRedeemCodeRequested(this->redeemCode);
}

void GameSettingsWidget::triggerUiConfiguratorRequest(void)
{
    if (!this->onStartUiConfiguratorRequested)
    {
        return;
    }

    this->onStartUiConfiguratorRequested();
}

std::size_t GameSettingsWidget::getRedeemCursorIndexFromPosition(float renderX, const SDL_FRect& inputRect) const
{
    const float inputTextX = inputRect.x + 10.0f;
    const float localX = (std::max)(0.0f, renderX - inputTextX);
    std::size_t newCursor = 0;
    for (std::size_t i = 0; i <= this->redeemCode.size(); ++i)
    {
        const float width = measureTextWidth(const_cast<RC2D_Font*>(&this->bodyFont), this->redeemCode.substr(0, i));
        if (localX <= width)
        {
            newCursor = i;
            break;
        }
        newCursor = i;
    }
    return (std::min)(newCursor, this->redeemCode.size());
}

bool GameSettingsWidget::hasRedeemSelection(void) const
{
    return this->getRedeemSelectionStart() != this->getRedeemSelectionEnd();
}

std::size_t GameSettingsWidget::getRedeemSelectionStart(void) const
{
    const std::size_t clampedCursor = (std::min)(this->redeemCursorIndex, this->redeemCode.size());
    const std::size_t clampedAnchor = (std::min)(this->redeemSelectionAnchorIndex, this->redeemCode.size());
    return (std::min)(clampedAnchor, clampedCursor);
}

std::size_t GameSettingsWidget::getRedeemSelectionEnd(void) const
{
    const std::size_t clampedCursor = (std::min)(this->redeemCursorIndex, this->redeemCode.size());
    const std::size_t clampedAnchor = (std::min)(this->redeemSelectionAnchorIndex, this->redeemCode.size());
    return (std::max)(clampedAnchor, clampedCursor);
}

void GameSettingsWidget::clearRedeemSelection(void)
{
    this->redeemSelectionAnchorIndex = (std::min)(this->redeemCursorIndex, this->redeemCode.size());
}

void GameSettingsWidget::deleteSelectedRedeemText(void)
{
    if (!this->hasRedeemSelection())
    {
        return;
    }

    const std::size_t selectionStart = this->getRedeemSelectionStart();
    const std::size_t selectionEnd = this->getRedeemSelectionEnd();
    this->redeemCode.erase(selectionStart, selectionEnd - selectionStart);
    this->redeemCursorIndex = selectionStart;
    this->clearRedeemSelection();
}

void GameSettingsWidget::applyHudScaleValue(HudScaleTarget target, float scale, bool notify)
{
    const std::size_t index = static_cast<std::size_t>(target);
    if (index >= this->hudScaleValues.size())
    {
        return;
    }

    const float clampedScale = snapHudScaleValue(target, scale);
    const float previousScale = this->hudScaleValues[index];
    this->hudScaleValues[index] = clampedScale;

    if (notify && this->onHudScaleChanged && std::fabs(previousScale - clampedScale) > 0.0001f)
    {
        this->onHudScaleChanged(target, clampedScale);
    }
}

void GameSettingsWidget::applyHudVisibilityValue(HudScaleTarget target, bool isVisible, bool notify)
{
    const std::size_t index = static_cast<std::size_t>(target);
    if (index >= this->hudVisibilityValues.size())
    {
        return;
    }

    const bool previousVisible = this->hudVisibilityValues[index];
    this->hudVisibilityValues[index] = isVisible;

    if (notify && this->onHudVisibilityChanged && previousVisible != isVisible)
    {
        this->onHudVisibilityChanged(target, isVisible);
    }
}

void GameSettingsWidget::updateDraggedHudScaleFromMouse(float mouseX)
{
    const std::size_t index = static_cast<std::size_t>(this->draggedHudScaleTarget);
    if (index >= this->hudScaleValues.size())
    {
        return;
    }

    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    const SDL_FRect& trackRect = layout.hudScaleTracks[index];
    if (trackRect.w <= 0.0f)
    {
        return;
    }

    const float thumbLeft = clampf(
        mouseX - this->hudScaleDragGrabOffsetX,
        trackRect.x - (kHudScaleThumbWidth * 0.5f),
        trackRect.x + trackRect.w - (kHudScaleThumbWidth * 0.5f));
    const float thumbCenterX = thumbLeft + (kHudScaleThumbWidth * 0.5f);
    const float normalized = (thumbCenterX - trackRect.x) / trackRect.w;
    this->applyHudScaleValue(this->draggedHudScaleTarget, normalizedToHudScaleValue(this->draggedHudScaleTarget, normalized), true);
}

void GameSettingsWidget::applyControlActionScancode(ControlAction action, SDL_Scancode scancode)
{
    const std::size_t index = static_cast<std::size_t>(action);
    if (index >= this->controlActionScancodes.size() || !isBindableScancode(scancode))
    {
        return;
    }

    this->controlActionScancodes[index] = scancode;
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

bool GameSettingsWidget::findControlActionUsingScancode(
    SDL_Scancode scancode,
    ControlAction excludedAction,
    ControlAction* outAction) const
{
    if (scancode == SDL_SCANCODE_UNKNOWN)
    {
        return false;
    }

    const std::size_t excludedIndex = static_cast<std::size_t>(excludedAction);
    for (std::size_t index = 0; index < this->controlActionScancodes.size(); ++index)
    {
        if (index == excludedIndex)
        {
            continue;
        }
        if (this->controlActionScancodes[index] != scancode)
        {
            continue;
        }

        if (outAction != nullptr)
        {
            *outAction = static_cast<ControlAction>(index);
        }
        return true;
    }

    return false;
}

void GameSettingsWidget::beginControlConflictConfirmation(
    ControlAction targetAction,
    ControlAction conflictAction,
    SDL_Scancode scancode)
{
    this->controlConflictPending = true;
    this->pendingConflictAction = targetAction;
    this->conflictingControlAction = conflictAction;
    this->pendingConflictScancode = scancode;
    this->controlCaptureActive = false;
}

void GameSettingsWidget::confirmPendingControlConflict(void)
{
    if (!this->controlConflictPending || !isBindableScancode(this->pendingConflictScancode))
    {
        this->cancelPendingControlConflict();
        return;
    }

    const std::size_t targetIndex = static_cast<std::size_t>(this->pendingConflictAction);
    if (targetIndex >= this->controlActionScancodes.size())
    {
        this->cancelPendingControlConflict();
        return;
    }

    for (std::size_t index = 0; index < this->controlActionScancodes.size(); ++index)
    {
        if (index != targetIndex && this->controlActionScancodes[index] == this->pendingConflictScancode)
        {
            this->controlActionScancodes[index] = SDL_SCANCODE_UNKNOWN;
        }
    }

    this->controlActionScancodes[targetIndex] = this->pendingConflictScancode;
    this->cancelPendingControlConflict();
    if (this->onUserSettingsChanged)
    {
        this->onUserSettingsChanged();
    }
}

void GameSettingsWidget::cancelPendingControlConflict(void)
{
    this->controlConflictPending = false;
    this->pendingConflictAction = ControlAction::CAMERA_MOVE_UP;
    this->conflictingControlAction = ControlAction::CAMERA_MOVE_UP;
    this->pendingConflictScancode = SDL_SCANCODE_UNKNOWN;
    this->controlCaptureActive = false;
}

void GameSettingsWidget::resetControlActionScancode(ControlAction action)
{
    const std::size_t index = static_cast<std::size_t>(action);
    if (index >= this->controlActionScancodes.size() || index >= kDefaultControlScancodes.size())
    {
        return;
    }

    const SDL_Scancode defaultScancode = kDefaultControlScancodes[index];
    if (defaultScancode != SDL_SCANCODE_UNKNOWN)
    {
        for (std::size_t otherIndex = 0; otherIndex < this->controlActionScancodes.size(); ++otherIndex)
        {
            if (otherIndex != index && this->controlActionScancodes[otherIndex] == defaultScancode)
            {
                this->controlActionScancodes[otherIndex] = SDL_SCANCODE_UNKNOWN;
            }
        }
    }

    this->controlActionScancodes[index] = defaultScancode;
}

void GameSettingsWidget::resetAllControlActionScancodes(void)
{
    for (std::size_t index = 0; index < this->controlActionScancodes.size(); ++index)
    {
        this->controlActionScancodes[index] = kDefaultControlScancodes[index];
    }
}

void GameSettingsWidget::clampControlsScroll(void)
{
    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    this->controlsScrollOffsetY =
        clampf(this->controlsScrollOffsetY, 0.0f, getControlsMaxScrollOffset(layout));
}

void GameSettingsWidget::updateControlsScrollFromMouse(float mouseY)
{
    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    const float maxScrollOffset = getControlsMaxScrollOffset(layout);
    if (maxScrollOffset <= 0.0f)
    {
        this->controlsScrollOffsetY = 0.0f;
        this->controlsScrollDragging = false;
        return;
    }

    const SDL_FRect thumbRect = getControlsScrollThumbRect(layout, this->controlsScrollOffsetY);
    const float thumbTravel = (std::max)(1.0f, layout.controlsScrollTrack.h - thumbRect.h);
    const float thumbTop = clampf(
        mouseY - this->controlsScrollDragOffsetY,
        layout.controlsScrollTrack.y,
        layout.controlsScrollTrack.y + thumbTravel);
    const float scrollRatio = (thumbTop - layout.controlsScrollTrack.y) / thumbTravel;
    this->controlsScrollOffsetY = scrollRatio * maxScrollOffset;
    this->clampControlsScroll();
}

void GameSettingsWidget::clampGraphicsScroll(void)
{
    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    this->graphicsScrollOffsetY =
        clampf(this->graphicsScrollOffsetY, 0.0f, getGraphicsMaxScrollOffset(layout));
}

void GameSettingsWidget::updateGraphicsScrollFromMouse(float mouseY)
{
    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    const float maxScrollOffset = getGraphicsMaxScrollOffset(layout);
    if (maxScrollOffset <= 0.0f)
    {
        this->graphicsScrollOffsetY = 0.0f;
        this->graphicsScrollDragging = false;
        return;
    }

    const SDL_FRect thumbRect = getGraphicsScrollThumbRect(layout, this->graphicsScrollOffsetY);
    const float thumbTravel = (std::max)(1.0f, layout.graphicsScrollTrack.h - thumbRect.h);
    const float thumbTop = clampf(
        mouseY - this->graphicsScrollDragOffsetY,
        layout.graphicsScrollTrack.y,
        layout.graphicsScrollTrack.y + thumbTravel);
    const float scrollRatio = (thumbTop - layout.graphicsScrollTrack.y) / thumbTravel;
    this->graphicsScrollOffsetY = scrollRatio * maxScrollOffset;
    this->clampGraphicsScroll();
}

void GameSettingsWidget::updateDraggedCameraScrollSpeedFromMouse(float mouseX)
{
    const GameSettingsLayout layout = buildLayout(this->widgetRect, static_cast<int>(this->languageOptions.size()));
    float entryY = layout.controlsViewport.y - this->controlsScrollOffsetY;
    for (const GameSettingsControlsEntry& entry : kControlsEntries)
    {
        const float entryHeight = getControlsEntryHeight(entry.type);
        if (entry.type == ControlsEntryType::CAMERA_SPEED)
        {
            const SDL_FRect rowRect = SDL_FRect{
                layout.controlsViewport.x,
                entryY,
                layout.controlsViewport.w,
                entryHeight
            };
            const SDL_FRect resetButtonRect = SDL_FRect{
                rowRect.x + rowRect.w - kControlsResetButtonWidth,
                rowRect.y + ((rowRect.h - kControlsButtonHeight) * 0.5f),
                kControlsResetButtonWidth,
                kControlsButtonHeight
            };
            const SDL_FRect trackRect = SDL_FRect{
                rowRect.x + 292.0f,
                rowRect.y + ((rowRect.h - kControlsTrackHeight) * 0.5f),
                (std::max)(1.0f, resetButtonRect.x - 70.0f - (rowRect.x + 292.0f)),
                kControlsTrackHeight
            };
            const float thumbLeft = clampf(
                mouseX - this->cameraScrollSpeedDragGrabOffsetX,
                trackRect.x - (kControlsThumbWidth * 0.5f),
                trackRect.x + trackRect.w - (kControlsThumbWidth * 0.5f));
            const float thumbCenterX = thumbLeft + (kControlsThumbWidth * 0.5f);
            const float normalized = (thumbCenterX - trackRect.x) / trackRect.w;
            this->setCameraScrollSpeedSectors(normalizedToCameraScrollSpeed(normalized));
            return;
        }

        entryY += entryHeight + kControlsRowGap;
    }
}

GameSettingsWidget::GraphicsWindowMode GameSettingsWidget::getGraphicsWindowMode(void) const
{
    const RC2D_FullscreenInfo fullscreenInfo = rc2d_window_getFullscreen();
    if (!fullscreenInfo.is_fullscreen)
    {
        return GraphicsWindowMode::MAXIMIZED_WINDOW;
    }

    if (fullscreenInfo.type == RC2D_FULLSCREEN_BORDERLESS)
    {
        return GraphicsWindowMode::FULLSCREEN_BORDERLESS;
    }

    if (fullscreenInfo.type == RC2D_FULLSCREEN_EXCLUSIVE)
    {
        return GraphicsWindowMode::FULLSCREEN_EXCLUSIVE;
    }

    return GraphicsWindowMode::FULLSCREEN_EXCLUSIVE;
}

void GameSettingsWidget::applyGraphicsWindowMode(GraphicsWindowMode mode)
{
    if (mode == GraphicsWindowMode::MAXIMIZED_WINDOW)
    {
        rc2d_window_setFullscreen(false, RC2D_FULLSCREEN_NONE, true);
        rc2d_window_maximize();
        return;
    }

    if (mode == GraphicsWindowMode::FULLSCREEN_BORDERLESS)
    {
        rc2d_window_setFullscreen(true, RC2D_FULLSCREEN_BORDERLESS, true);
        return;
    }

    rc2d_window_setFullscreen(true, RC2D_FULLSCREEN_EXCLUSIVE, true);
}

RC2D_LogicalPresentationMode GameSettingsWidget::getGraphicsPresentationMode(void) const
{
    const RC2D_LogicalPresentationMode mode = rc2d_engine_getLogicalPresentationMode();
    if (mode == RC2D_LOGICAL_PRESENTATION_LETTERBOX)
    {
        return RC2D_LOGICAL_PRESENTATION_LETTERBOX;
    }

    return RC2D_LOGICAL_PRESENTATION_OVERSCAN;
}

void GameSettingsWidget::applyGraphicsPresentationMode(RC2D_LogicalPresentationMode mode)
{
    const RC2D_LogicalPresentationMode appliedMode =
        (mode == RC2D_LOGICAL_PRESENTATION_LETTERBOX)
            ? RC2D_LOGICAL_PRESENTATION_LETTERBOX
            : RC2D_LOGICAL_PRESENTATION_OVERSCAN;
    (void)rc2d_engine_setLogicalPresentationMode(appliedMode);
}
