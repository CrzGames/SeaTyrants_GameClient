#include "game/ui/hud/account-management-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

static constexpr float kRefW = 1030.0f;
static constexpr float kRefH = 700.0f;

static constexpr float kTopBarHeight = 34.0f;
static constexpr float kScrollBarWidth = 8.0f;
static constexpr float kScrollBarPadding = 4.0f;
static constexpr float kMinThumbHeight = 22.0f;
static constexpr float kFleetCardW = 126.0f;
static constexpr float kFleetCardH = 150.0f;
static constexpr float kFleetCardGap = 6.0f;
/** Marge verticale identique entre le viewport et la zone des cartes (haut et bas). */
static constexpr float kFleetVerticalPadding = 6.0f;
static constexpr float kPickerRowHeight = 36.0f;
static constexpr int kPickerMaxVisibleRows = 7;
static constexpr int kEffectPickerMaxVisibleRows = 5;
static constexpr int kMaxProfileNameChars = 20;
static constexpr double kCursorBlinkPeriod = 0.55;

static constexpr RC2D_Color kPanelFill = RC2D_Color{4, 9, 20, 242};
static constexpr RC2D_Color kGold = RC2D_Color{184, 132, 30, 250};
static constexpr RC2D_Color kSilver = RC2D_Color{211, 214, 220, 232};
static constexpr RC2D_Color kHeaderFill = RC2D_Color{67, 8, 8, 234};
static constexpr RC2D_Color kTabInactive = RC2D_Color{26, 20, 14, 238};
static constexpr RC2D_Color kTabActive = RC2D_Color{54, 28, 8, 242};
static constexpr RC2D_Color kFieldFill = RC2D_Color{12, 12, 14, 236};
static constexpr RC2D_Color kButtonFill = RC2D_Color{7, 35, 52, 236};
static constexpr RC2D_Color kTextGold = RC2D_Color{217, 200, 134, 255};
static constexpr RC2D_Color kTextBody = RC2D_Color{210, 215, 225, 255};
static constexpr RC2D_Color kTextMuted = RC2D_Color{126, 132, 142, 255};
static constexpr RC2D_Color kRowLine = RC2D_Color{134, 102, 39, 220};
static constexpr RC2D_Color kScrollTrack = RC2D_Color{26, 29, 33, 235};
static constexpr RC2D_Color kScrollThumb = RC2D_Color{124, 132, 142, 240};
static constexpr RC2D_Color kScrollThumbDragFill = RC2D_Color{184, 132, 30, 245};
static constexpr float kScrollThumbWheelHighlightSec = 0.25f;
static constexpr RC2D_Color kSelectionFill = RC2D_Color{28, 33, 44, 240};

struct WidgetLayout {
    SDL_FRect outer;
    SDL_FRect inner;
    SDL_FRect topBar;
    SDL_FRect closeButton;
    SDL_FRect tabAccount;
    SDL_FRect tabAppearance;
    SDL_FRect tabShipManagement;
    SDL_FRect tabElite;
    SDL_FRect tabSpecial;
    SDL_FRect tabStorageEquipped;
    SDL_FRect headerDragRect;
    SDL_FRect content;

    SDL_FRect accountLeftInfo;
    SDL_FRect accountLeftStats;
    SDL_FRect accountLeftPremium;
    SDL_FRect accountRightProfile;
    SDL_FRect accountProfileNameInput;
    SDL_FRect accountProfileApplyButton;

    SDL_FRect shipManagementHeader;
    SDL_FRect shipManagementShipCase;

    SDL_FRect appearanceShipHeader;
    SDL_FRect appearanceShipCase;
    SDL_FRect appearanceCoatingHeader;
    SDL_FRect appearanceCoatingCase;
    SDL_FRect appearanceEffectsHeader;
    SDL_FRect appearanceRepairCase;
    SDL_FRect appearanceSpeedCase;
    SDL_FRect appearanceExplosionCase;
    SDL_FRect appearanceRocketCase;
    SDL_FRect appearanceProjectileCase;
    SDL_FRect appearanceMoveClickCase;
    SDL_FRect appearanceEmotesHeader;
    SDL_FRect appearanceEmotesCase;
    std::array<SDL_FRect, 6> appearanceEmoteSlots;

    SDL_FRect fleetHeader;
    SDL_FRect fleetPointsPanel;
    SDL_FRect fleetGridViewportElite;
    SDL_FRect fleetGridViewportSpecial;

    SDL_FRect storageLeftPanel;
    SDL_FRect storageLeftDropdown;
    SDL_FRect storageLeftContent;
    SDL_FRect storageMiddlePanel;
    SDL_FRect storageMiddleDropdown;
    SDL_FRect storageMiddleContent;
    SDL_FRect storageRightPanel;
};

struct FleetMetrics {
    int columns;
    int totalRows;
    int visibleRows;
    int maxFirstRow;
    float cardStartX;
    float cardStrideY;
    float contentWidth;
    /** Decalage depuis viewport.y jusqu'au haut de la premiere rangee de cartes (marges haut/bas symetriques). */
    float fleetContentTopOffset;
    SDL_FRect scrollTrack;
};

struct PickerLayout {
    SDL_FRect panel;
    SDL_FRect body;
    SDL_FRect scrollTrack;
    int totalRows;
    int visibleRows;
    int maxFirstRow;
    /** True seulement si une scrollbar verticale est affichee (maxFirstRow > 0). */
    bool showScrollBar;
};

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
    rc2d_graphics_drawText(
        &t,
        std::round(r.x + ((r.w - static_cast<float>(w)) * 0.5f)),
        std::round(r.y + ((r.h - static_cast<float>(h)) * 0.5f)));
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

static bool isImageFileAssetPath(const std::string& assetPath)
{
    const std::size_t extensionPos = assetPath.find_last_of('.');
    const std::size_t separatorPos = assetPath.find_last_of("/\\");
    if (extensionPos == std::string::npos ||
        (separatorPos != std::string::npos && extensionPos < separatorPos))
    {
        return false;
    }

    std::string extension = assetPath.substr(extensionPos);
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    return extension == ".png" ||
           extension == ".jpg" ||
           extension == ".jpeg" ||
           extension == ".bmp" ||
           extension == ".webp";
}

static std::string resolvePreviewAssetPath(const std::string& assetPath)
{
    if (assetPath.empty())
    {
        return "";
    }

    if (isImageFileAssetPath(assetPath))
    {
        return assetPath;
    }

    const char lastCharacter = assetPath.back();
    if (lastCharacter == '/' || lastCharacter == '\\')
    {
        return assetPath + "1.png";
    }

    return assetPath + "/1.png";
}

static RC2D_Image loadImageFromTitleOrEmpty(const std::string& assetPath)
{
    const std::string resolvedAssetPath = resolvePreviewAssetPath(assetPath);
    if (resolvedAssetPath.empty())
    {
        return RC2D_Image{};
    }

    return LoadStorageImage(resolvedAssetPath.c_str(), RC2D_STORAGE_TITLE);
}

static void drawImageFit(const RC2D_Image* image, const SDL_FRect& target)
{
    if (image == nullptr || image->sdl_texture == nullptr)
    {
        return;
    }

    float texW = 0.0f;
    float texH = 0.0f;
    if (!SDL_GetTextureSize(image->sdl_texture, &texW, &texH) || texW <= 0.0f || texH <= 0.0f)
    {
        return;
    }

    const float scale = (std::min)(target.w / texW, target.h / texH);
    const float drawW = texW * scale;
    const float drawH = texH * scale;
    const float drawX = target.x + ((target.w - drawW) * 0.5f);
    const float drawY = target.y + ((target.h - drawH) * 0.5f);

    RC2D_Image imageCopy = *image;
    const RC2D_Quad quad = rc2d_graphics_newQuad(&imageCopy, 0.0f, 0.0f, texW, texH);
    if (quad.src.w <= 0.0f || quad.src.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_drawQuad(&imageCopy, &quad, drawX, drawY, 0.0, scale, scale, -1.0f, -1.0f, false, false);
}

static std::string formatWithDots(int value)
{
    if (value <= 0)
    {
        return "0";
    }

    std::string digits = std::to_string(value);
    std::string output;
    output.reserve(digits.size() + (digits.size() / 3));

    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i)
    {
        output.push_back(digits[static_cast<std::size_t>(i)]);
        ++count;
        if (count == 3 && i > 0)
        {
            output.push_back('.');
            count = 0;
        }
    }
    std::reverse(output.begin(), output.end());
    return output;
}

static std::string clampProfileNameToUiLimit(const std::string& profileName)
{
    const std::size_t maxLength = static_cast<std::size_t>(kMaxProfileNameChars);
    if (profileName.size() <= maxLength)
    {
        return profileName;
    }

    return profileName.substr(0, maxLength);
}

static WidgetLayout buildLayout(const SDL_FRect& outer)
{
    WidgetLayout layout{};
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
    const float tabGap = 2.0f;
    layout.tabAccount = SDL_FRect{outer.x + 10.0f, tabY, 90.0f, tabH};
    layout.tabAppearance = SDL_FRect{layout.tabAccount.x + layout.tabAccount.w + tabGap, tabY, 108.0f, tabH};
    layout.tabShipManagement = SDL_FRect{layout.tabAppearance.x + layout.tabAppearance.w + tabGap, tabY, 146.0f, tabH};
    layout.tabElite = SDL_FRect{layout.tabShipManagement.x + layout.tabShipManagement.w + tabGap, tabY, 162.0f, tabH};
    layout.tabSpecial = SDL_FRect{layout.tabElite.x + layout.tabElite.w + tabGap, tabY, 182.0f, tabH};
    layout.tabStorageEquipped = SDL_FRect{layout.tabSpecial.x + layout.tabSpecial.w + tabGap, tabY, 160.0f, tabH};

    const float dragLeft = layout.tabStorageEquipped.x + layout.tabStorageEquipped.w + 4.0f;
    const float dragRight = layout.closeButton.x - 4.0f;
    if (dragRight > dragLeft)
    {
        layout.headerDragRect = SDL_FRect{dragLeft, layout.topBar.y, dragRight - dragLeft, layout.topBar.h};
    }
    else
    {
        layout.headerDragRect = layout.topBar;
    }

    const float contentY = layout.topBar.y + layout.topBar.h + 8.0f;
    layout.content = SDL_FRect{outer.x + 8.0f, contentY, outer.w - 16.0f, (outer.y + outer.h) - contentY - 8.0f};

    const float accountLeftW = 420.0f;
    const float accountGap = 4.0f;
    const float accountTopInset = 14.0f;
    const float accountInfoH = 132.0f;
    const float accountStatsH = 180.0f;
    const float accountBaseY = layout.content.y + accountTopInset;
    const float accountAvailableH = (std::max)(120.0f, layout.content.h - accountTopInset);
    const float accountPremiumH = (std::max)(64.0f, accountAvailableH - accountInfoH - accountStatsH - (accountGap * 2.0f));
    layout.accountLeftInfo = SDL_FRect{layout.content.x, accountBaseY, accountLeftW, accountInfoH};
    layout.accountLeftStats = SDL_FRect{layout.content.x, layout.accountLeftInfo.y + layout.accountLeftInfo.h + accountGap, accountLeftW, accountStatsH};
    layout.accountLeftPremium = SDL_FRect{layout.content.x, layout.accountLeftStats.y + layout.accountLeftStats.h + accountGap, accountLeftW, accountPremiumH};

    const float accountRightX = layout.accountLeftInfo.x + layout.accountLeftInfo.w + accountGap;
    layout.accountRightProfile = SDL_FRect{accountRightX, accountBaseY, layout.content.w - accountLeftW - accountGap, accountAvailableH};
    layout.accountProfileNameInput = SDL_FRect{
        layout.accountRightProfile.x + ((layout.accountRightProfile.w - 176.0f) * 0.5f),
        layout.accountRightProfile.y + 250.0f,
        176.0f,
        30.0f
    };
    layout.accountProfileApplyButton = SDL_FRect{
        layout.accountProfileNameInput.x,
        layout.accountProfileNameInput.y + layout.accountProfileNameInput.h + 10.0f,
        layout.accountProfileNameInput.w,
        32.0f
    };

    layout.shipManagementHeader = SDL_FRect{layout.content.x, layout.content.y, layout.content.w, 30.0f};
    layout.shipManagementShipCase = SDL_FRect{
        layout.content.x,
        layout.shipManagementHeader.y + layout.shipManagementHeader.h,
        layout.content.w,
        96.0f
    };

    const float sectionGap = 2.0f;
    const float appearanceCaseH = 96.0f;
    const float appearanceColumnGap = 2.0f;
    const float appearanceColumnW = (layout.content.w - appearanceColumnGap) * 0.5f;
    const float appearanceLeftX = layout.content.x;
    const float appearanceRightX = appearanceLeftX + appearanceColumnW + appearanceColumnGap;

    // La premiere ligne "Navire" est retiree de l'onglet Apparence.
    layout.appearanceShipHeader = SDL_FRect{layout.content.x, layout.content.y, layout.content.w, 0.0f};
    layout.appearanceShipCase = SDL_FRect{layout.content.x, layout.content.y, layout.content.w, 0.0f};

    layout.appearanceCoatingHeader = SDL_FRect{
        layout.content.x,
        layout.content.y,
        layout.content.w,
        30.0f
    };
    layout.appearanceCoatingCase = SDL_FRect{
        layout.content.x,
        layout.appearanceCoatingHeader.y + layout.appearanceCoatingHeader.h,
        layout.content.w,
        appearanceCaseH
    };

    layout.appearanceEffectsHeader = SDL_FRect{
        layout.content.x,
        layout.appearanceCoatingCase.y + layout.appearanceCoatingCase.h + sectionGap,
        layout.content.w,
        30.0f
    };

    const float effectsRow1Y = layout.appearanceEffectsHeader.y + layout.appearanceEffectsHeader.h;
    const float effectsRow2Y = effectsRow1Y + appearanceCaseH + sectionGap;
    const float effectsRow3Y = effectsRow2Y + appearanceCaseH + sectionGap;

    layout.appearanceRepairCase = SDL_FRect{appearanceLeftX, effectsRow1Y, appearanceColumnW, appearanceCaseH};
    layout.appearanceSpeedCase = SDL_FRect{appearanceRightX, effectsRow1Y, appearanceColumnW, appearanceCaseH};
    layout.appearanceExplosionCase = SDL_FRect{appearanceLeftX, effectsRow2Y, appearanceColumnW, appearanceCaseH};
    layout.appearanceRocketCase = SDL_FRect{appearanceRightX, effectsRow2Y, appearanceColumnW, appearanceCaseH};
    layout.appearanceProjectileCase = SDL_FRect{appearanceLeftX, effectsRow3Y, appearanceColumnW, appearanceCaseH};
    layout.appearanceMoveClickCase = SDL_FRect{appearanceRightX, effectsRow3Y, appearanceColumnW, appearanceCaseH};

    layout.appearanceEmotesHeader = SDL_FRect{
        layout.content.x,
        layout.appearanceProjectileCase.y + layout.appearanceProjectileCase.h + sectionGap,
        layout.content.w,
        30.0f
    };
    const float emotesY = layout.appearanceEmotesHeader.y + layout.appearanceEmotesHeader.h;
    const float emotesH = (std::max)(90.0f, (layout.content.y + layout.content.h) - emotesY);
    layout.appearanceEmotesCase = SDL_FRect{layout.content.x, emotesY, layout.content.w, emotesH};

    const float emoteSlotSize = 78.0f;
    const float emoteGap = 14.0f;
    const float emoteTotalW = (emoteSlotSize * 6.0f) + (emoteGap * 5.0f);
    float emoteStartX = layout.appearanceEmotesCase.x + ((layout.appearanceEmotesCase.w - emoteTotalW) * 0.5f);
    const float emoteY = layout.appearanceEmotesCase.y + (std::max)(6.0f, ((layout.appearanceEmotesCase.h - emoteSlotSize) * 0.5f));
    for (int i = 0; i < static_cast<int>(layout.appearanceEmoteSlots.size()); ++i)
    {
        layout.appearanceEmoteSlots[static_cast<std::size_t>(i)] = SDL_FRect{
            emoteStartX + (static_cast<float>(i) * (emoteSlotSize + emoteGap)),
            emoteY,
            emoteSlotSize,
            emoteSlotSize
        };
    }

    layout.fleetHeader = SDL_FRect{layout.content.x, layout.content.y, layout.content.w, 30.0f};
    layout.fleetPointsPanel = SDL_FRect{layout.content.x, layout.fleetHeader.y + layout.fleetHeader.h + 3.0f, layout.content.w, 48.0f};
    layout.fleetGridViewportElite = SDL_FRect{
        layout.content.x,
        layout.fleetPointsPanel.y + layout.fleetPointsPanel.h + 4.0f,
        layout.content.w,
        (layout.content.y + layout.content.h) - (layout.fleetPointsPanel.y + layout.fleetPointsPanel.h + 4.0f)
    };
    layout.fleetGridViewportSpecial = SDL_FRect{
        layout.content.x,
        layout.fleetHeader.y + layout.fleetHeader.h + 4.0f,
        layout.content.w,
        (layout.content.y + layout.content.h) - (layout.fleetHeader.y + layout.fleetHeader.h + 4.0f)
    };

    const float storageTopInset = 14.0f;
    const float storagePanelY = layout.content.y + storageTopInset;
    const float storagePanelH = (std::max)(100.0f, layout.content.h - storageTopInset);
    const float storageGap = 2.0f;
    const float storageColW = (layout.content.w - (storageGap * 2.0f)) / 3.0f;
    const float storageDropH = 34.0f;

    layout.storageLeftPanel = SDL_FRect{layout.content.x, storagePanelY, storageColW, storagePanelH};
    layout.storageMiddlePanel = SDL_FRect{layout.storageLeftPanel.x + storageColW + storageGap, storagePanelY, storageColW, storagePanelH};
    layout.storageRightPanel = SDL_FRect{layout.storageMiddlePanel.x + storageColW + storageGap, storagePanelY, storageColW, storagePanelH};

    layout.storageLeftDropdown = SDL_FRect{
        layout.storageLeftPanel.x + 1.0f,
        layout.storageLeftPanel.y + 1.0f,
        layout.storageLeftPanel.w - 2.0f,
        storageDropH
    };
    layout.storageMiddleDropdown = SDL_FRect{
        layout.storageMiddlePanel.x + 1.0f,
        layout.storageMiddlePanel.y + 1.0f,
        layout.storageMiddlePanel.w - 2.0f,
        storageDropH
    };
    layout.storageLeftContent = SDL_FRect{
        layout.storageLeftPanel.x + 1.0f,
        layout.storageLeftDropdown.y + layout.storageLeftDropdown.h + 1.0f,
        layout.storageLeftPanel.w - 2.0f,
        layout.storageLeftPanel.h - layout.storageLeftDropdown.h - 2.0f
    };
    layout.storageMiddleContent = SDL_FRect{
        layout.storageMiddlePanel.x + 1.0f,
        layout.storageMiddleDropdown.y + layout.storageMiddleDropdown.h + 1.0f,
        layout.storageMiddlePanel.w - 2.0f,
        layout.storageMiddlePanel.h - layout.storageMiddleDropdown.h - 2.0f
    };

    return layout;
}

static FleetMetrics buildFleetMetrics(const SDL_FRect& viewport, int totalItems)
{
    FleetMetrics metrics{};
    metrics.columns = 1;
    metrics.totalRows = 0;
    metrics.visibleRows = 1;
    metrics.maxFirstRow = 0;
    metrics.cardStrideY = kFleetCardH + kFleetCardGap;
    metrics.cardStartX = viewport.x + 2.0f;
    metrics.contentWidth = viewport.w;
    metrics.fleetContentTopOffset = kFleetVerticalPadding;
    metrics.scrollTrack = SDL_FRect{
        viewport.x + viewport.w - (kScrollBarWidth + kScrollBarPadding),
        viewport.y + kScrollBarPadding,
        kScrollBarWidth,
        viewport.h - (kScrollBarPadding * 2.0f)
    };

    const float innerH = (std::max)(0.0f, viewport.h - (2.0f * kFleetVerticalPadding));
    metrics.visibleRows = (std::max)(
        1,
        static_cast<int>(std::floor((innerH + kFleetCardGap) / metrics.cardStrideY)));
    const float contentHeight =
        (static_cast<float>(metrics.visibleRows) * metrics.cardStrideY) - kFleetCardGap;
    const float verticalCenterFiller =
        (innerH > 0.0f) ? ((innerH - contentHeight) * 0.5f) : 0.0f;
    metrics.fleetContentTopOffset =
        kFleetVerticalPadding + (std::max)(0.0f, verticalCenterFiller);

    const float scrollReserve = kScrollBarWidth + (kScrollBarPadding * 2.0f);
    const float availableWNoScroll = (std::max)(80.0f, viewport.w - 2.0f);
    const int columnsNoScroll = (std::max)(1, static_cast<int>(std::floor((availableWNoScroll + kFleetCardGap) / (kFleetCardW + kFleetCardGap))));
    const int totalRowsNoScroll = (totalItems <= 0)
        ? 0
        : static_cast<int>((totalItems + columnsNoScroll - 1) / columnsNoScroll);
    const bool needsScroll = totalRowsNoScroll > metrics.visibleRows;

    const float availableW = needsScroll
        ? (std::max)(80.0f, viewport.w - scrollReserve - 2.0f)
        : availableWNoScroll;

    metrics.contentWidth = availableW;
    metrics.columns = (std::max)(1, static_cast<int>(std::floor((availableW + kFleetCardGap) / (kFleetCardW + kFleetCardGap))));
    metrics.totalRows = (totalItems <= 0)
        ? 0
        : static_cast<int>((totalItems + metrics.columns - 1) / metrics.columns);
    metrics.maxFirstRow = (std::max)(0, metrics.totalRows - metrics.visibleRows);

    const float usedW = (static_cast<float>(metrics.columns) * kFleetCardW) + (static_cast<float>((std::max)(0, metrics.columns - 1)) * kFleetCardGap);
    metrics.cardStartX = viewport.x + ((availableW - usedW) * 0.5f);

    return metrics;
}

static PickerLayout buildPickerLayout(
    const SDL_FRect& widgetRect,
    const SDL_FRect& anchorRect,
    int totalRows,
    int preferredMaxVisibleRows,
    bool openUpward)
{
    PickerLayout layout{};
    layout.totalRows = (std::max)(0, totalRows);
    const float panelW = (std::max)(260.0f, anchorRect.w);
    const int capRows = (std::max)(1, preferredMaxVisibleRows);
    const int wantedRows = (std::max)(1, (std::min)(capRows, (std::max)(1, layout.totalRows)));
    layout.visibleRows = wantedRows;

    float panelX = anchorRect.x;
    const float widgetTopLimit = widgetRect.y + 8.0f;
    const float widgetBottomLimit = widgetRect.y + widgetRect.h - 8.0f;

    if (openUpward)
    {
        const float availableAbove = (anchorRect.y - 4.0f) - widgetTopLimit;
        const int maxRowsAbove = (std::max)(
            1,
            static_cast<int>(std::floor((std::max)(0.0f, availableAbove - 8.0f) / kPickerRowHeight)));
        layout.visibleRows = (std::min)(layout.visibleRows, maxRowsAbove);
    }
    else
    {
        const float defaultPanelY = anchorRect.y + anchorRect.h + 4.0f;
        const float availableBelow = widgetBottomLimit - defaultPanelY;
        const int maxRowsBelow = (std::max)(
            1,
            static_cast<int>(std::floor((std::max)(0.0f, availableBelow - 8.0f) / kPickerRowHeight)));
        layout.visibleRows = (std::min)(layout.visibleRows, maxRowsBelow);
    }

    layout.visibleRows = (std::max)(1, layout.visibleRows);
    layout.maxFirstRow = (std::max)(0, layout.totalRows - layout.visibleRows);

    const float panelH = (static_cast<float>(layout.visibleRows) * kPickerRowHeight) + 8.0f;
    float panelY = openUpward
        ? (anchorRect.y - panelH - 4.0f)
        : (anchorRect.y + anchorRect.h + 4.0f);

    if (panelX + panelW > widgetRect.x + widgetRect.w - 8.0f)
    {
        panelX = (widgetRect.x + widgetRect.w - 8.0f) - panelW;
    }
    if (panelX < widgetRect.x + 8.0f)
    {
        panelX = widgetRect.x + 8.0f;
    }

    if (!openUpward && panelY + panelH > widgetBottomLimit)
    {
        panelY = widgetBottomLimit - panelH;
    }
    if (openUpward && panelY < widgetTopLimit)
    {
        panelY = widgetTopLimit;
    }
    if (panelY < widgetTopLimit)
    {
        panelY = widgetTopLimit;
    }
    if (panelY + panelH > widgetBottomLimit)
    {
        panelY = widgetBottomLimit - panelH;
    }

    layout.panel = SDL_FRect{panelX, panelY, panelW, panelH};
    layout.body = SDL_FRect{layout.panel.x + 4.0f, layout.panel.y + 4.0f, layout.panel.w - 8.0f, layout.panel.h - 8.0f};
    layout.scrollTrack = SDL_FRect{
        layout.body.x + layout.body.w - (kScrollBarWidth + kScrollBarPadding),
        layout.body.y + kScrollBarPadding,
        kScrollBarWidth,
        layout.body.h - (kScrollBarPadding * 2.0f)
    };

    layout.showScrollBar = layout.maxFirstRow > 0;
    return layout;
}

static float getPickerListRowWidth(const PickerLayout& layout)
{
    if (!layout.showScrollBar)
    {
        return layout.body.w;
    }
    return layout.body.w - (kScrollBarWidth + (kScrollBarPadding * 2.0f));
}

static char extractDigitFromKeyLabel(const char* key)
{
    if (key == nullptr || key[0] == '\0')
    {
        return '\0';
    }

    if (key[1] == '\0' && key[0] >= '0' && key[0] <= '9')
    {
        return key[0];
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

    if ((mod & SDL_KMOD_CTRL) != 0 || (mod & SDL_KMOD_ALT) != 0)
    {
        return false;
    }

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

    char digit = extractDigitFromKeyLabel(key);
    if (digit != '\0')
    {
        *outChar = digit;
        return true;
    }

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
        default:
            break;
    }

    return false;
}

static SDL_FRect getFleetCardRect(
    const SDL_FRect& viewport,
    const FleetMetrics& metrics,
    int sourceIndex,
    int firstRow)
{
    if (metrics.columns <= 0 || sourceIndex < 0)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const int row = sourceIndex / metrics.columns;
    const int column = sourceIndex % metrics.columns;
    const int visualRow = row - firstRow;
    if (visualRow < 0 || visualRow >= metrics.visibleRows)
    {
        return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const float x = metrics.cardStartX + (static_cast<float>(column) * (kFleetCardW + kFleetCardGap));
    const float y = viewport.y + metrics.fleetContentTopOffset + (static_cast<float>(visualRow) * metrics.cardStrideY);
    return SDL_FRect{x, y, kFleetCardW, kFleetCardH};
}

AccountManagementWidget::AccountManagementWidget(void)
    : titleFont{},
      bodyFont{},
      smallFont{},
      widgetRect{0.0f, 0.0f, kRefW, kRefH},
      visible(true),
      cursorEnabled(true),
      resourcesLoaded(false),
      controlIcons{},
      activeTab(ActiveTab::ACCOUNT),
      widgetDragging(false),
      widgetDragOffsetX(0.0f),
      widgetDragOffsetY(0.0f),
      widgetOffsetX(0.0f),
      widgetOffsetY(0.0f),
      eliteShips{},
      specialShips{},
      eliteShipIcons{},
      specialShipIcons{},
      playerIdentifier{},
      pirateSinceText{},
      playerLevel(0),
      experiencePointsCurrent(0),
      eliteProgressData{0, 0, false},
      combatPointsCurrent(0),
      premiumSinceText{},
      selectedEliteShip(0),
      selectedSpecialShip(0),
      eliteFirstRow(0),
      specialFirstRow(0),
      fleetScrollDragging(false),
      fleetScrollDragOffsetY(0.0f),
      fleetScrollWheelHighlightSec(0.0f),
      shipOptions{},
      coatingOptions{},
      repairStyleOptions{},
      speedStyleOptions{},
      explosionStyleOptions{},
      rocketStyleOptions{},
      projectileStyleOptions{},
      moveClickStyleOptions{},
      emoteOptions{},
      storageEquipmentOptions{},
      shipOptionIcons{},
      coatingOptionIcons{},
      repairStyleOptionIcons{},
      speedStyleOptionIcons{},
      explosionStyleOptionIcons{},
      rocketStyleOptionIcons{},
      projectileStyleOptionIcons{},
      moveClickStyleOptionIcons{},
      emoteOptionIcons{},
      storageEquipmentOptionIcons{},
      selectedShipOption(0),
      selectedCoatingOption(0),
      selectedRepairStyleOption(0),
      selectedSpeedStyleOption(0),
      selectedExplosionStyleOption(0),
      selectedRocketStyleOption(0),
      selectedProjectileStyleOption(0),
      selectedMoveClickStyleOption(0),
      selectedEmoteOption(0),
      selectedStorageEquipmentOption(0),
      openPicker(AppearancePickerType::NONE),
      openPickerAnchorRect{0.0f, 0.0f, 0.0f, 0.0f},
      pickerFirstRow(0),
      pickerScrollDragging(false),
      pickerScrollDragOffsetY(0.0f),
      pickerScrollWheelHighlightSec(0.0f),
      profileName{},
      profileInputFocused(false),
      profileCursorIndex(0),
      profileCursorVisible(false),
      profileCursorBlinkElapsed(0.0)
{
    this->profileCursorIndex = this->profileName.size();
}

AccountManagementWidget::~AccountManagementWidget(void)
{
}

void AccountManagementWidget::setEliteAcquiredShips(const std::vector<ShipEntry>& ships)
{
    this->setShipsForCollection(ShipCollectionType::ELITE_ACQUIRED, ships);
}

void AccountManagementWidget::addEliteAcquiredShip(const ShipEntry& ship)
{
    this->addShipToCollection(ShipCollectionType::ELITE_ACQUIRED, ship);
}

void AccountManagementWidget::clearEliteAcquiredShips(void)
{
    this->clearShipsForCollection(ShipCollectionType::ELITE_ACQUIRED);
}

void AccountManagementWidget::setSpecialAcquiredShips(const std::vector<ShipEntry>& ships)
{
    this->setShipsForCollection(ShipCollectionType::SPECIAL_ACQUIRED, ships);
}

void AccountManagementWidget::addSpecialAcquiredShip(const ShipEntry& ship)
{
    this->addShipToCollection(ShipCollectionType::SPECIAL_ACQUIRED, ship);
}

void AccountManagementWidget::clearSpecialAcquiredShips(void)
{
    this->clearShipsForCollection(ShipCollectionType::SPECIAL_ACQUIRED);
}

void AccountManagementWidget::setShipBonusOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::SHIP_BONUS, options);
}

void AccountManagementWidget::addShipBonusOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::SHIP_BONUS, option);
}

void AccountManagementWidget::clearShipBonusOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::SHIP_BONUS);
}

void AccountManagementWidget::setShipStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::SHIP_STYLE, options);
}

void AccountManagementWidget::addShipStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::SHIP_STYLE, option);
}

void AccountManagementWidget::clearShipStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::SHIP_STYLE);
}

void AccountManagementWidget::setRepairStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::REPAIR_STYLE, options);
}

void AccountManagementWidget::addRepairStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::REPAIR_STYLE, option);
}

void AccountManagementWidget::clearRepairStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::REPAIR_STYLE);
}

void AccountManagementWidget::setSpeedStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::SPEED_STYLE, options);
}

void AccountManagementWidget::addSpeedStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::SPEED_STYLE, option);
}

void AccountManagementWidget::clearSpeedStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::SPEED_STYLE);
}

void AccountManagementWidget::setProjectileImpactStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::PROJECTILE_IMPACT_STYLE, options);
}

void AccountManagementWidget::addProjectileImpactStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::PROJECTILE_IMPACT_STYLE, option);
}

void AccountManagementWidget::clearProjectileImpactStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::PROJECTILE_IMPACT_STYLE);
}

void AccountManagementWidget::setRocketStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::ROCKET_STYLE, options);
}

void AccountManagementWidget::addRocketStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::ROCKET_STYLE, option);
}

void AccountManagementWidget::clearRocketStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::ROCKET_STYLE);
}

void AccountManagementWidget::setProjectileStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::PROJECTILE_STYLE, options);
}

void AccountManagementWidget::addProjectileStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::PROJECTILE_STYLE, option);
}

void AccountManagementWidget::clearProjectileStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::PROJECTILE_STYLE);
}

void AccountManagementWidget::setMoveClickStyleOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::MOVE_CLICK_STYLE, options);
}

void AccountManagementWidget::addMoveClickStyleOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::MOVE_CLICK_STYLE, option);
}

void AccountManagementWidget::clearMoveClickStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::MOVE_CLICK_STYLE);
}

void AccountManagementWidget::setEmoteOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::EMOTE, options);
}

void AccountManagementWidget::addEmoteOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::EMOTE, option);
}

void AccountManagementWidget::clearEmoteOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::EMOTE);
}

void AccountManagementWidget::setStorageEquipmentOptions(const std::vector<OptionEntry>& options)
{
    this->setOptionsForCollection(OptionCollectionType::STORAGE_EQUIPMENT, options);
}

void AccountManagementWidget::addStorageEquipmentOption(const OptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::STORAGE_EQUIPMENT, option);
}

void AccountManagementWidget::clearStorageEquipmentOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::STORAGE_EQUIPMENT);
}

void AccountManagementWidget::setShipsForCollection(
    ShipCollectionType collection,
    const std::vector<ShipEntry>& ships)
{
    std::vector<ShipEntry>* target = this->getShipsForCollection(collection);
    if (target == nullptr)
    {
        return;
    }

    *target = ships;
    this->syncShipCollectionState(collection);

    if (this->resourcesLoaded)
    {
        this->loadShipIcons();
    }
}

void AccountManagementWidget::addShipToCollection(ShipCollectionType collection, const ShipEntry& ship)
{
    std::vector<ShipEntry>* target = this->getShipsForCollection(collection);
    if (target == nullptr)
    {
        return;
    }

    target->push_back(ship);
    this->syncShipCollectionState(collection);

    if (this->resourcesLoaded)
    {
        this->loadShipIcons();
    }
}

void AccountManagementWidget::clearShipsForCollection(ShipCollectionType collection)
{
    std::vector<ShipEntry>* target = this->getShipsForCollection(collection);
    if (target == nullptr)
    {
        return;
    }

    target->clear();
    this->syncShipCollectionState(collection);

    if (this->resourcesLoaded)
    {
        this->loadShipIcons();
    }
}

void AccountManagementWidget::setOptionsForCollection(
    OptionCollectionType collection,
    const std::vector<OptionEntry>& options)
{
    std::vector<AppearanceOption>* target = this->getOptionsForCollection(collection);
    if (target == nullptr)
    {
        return;
    }

    *target = options;
    this->syncOptionCollectionState(collection);

    if (this->resourcesLoaded)
    {
        this->loadAppearanceIcons();
    }
}

void AccountManagementWidget::addOptionToCollection(
    OptionCollectionType collection,
    const OptionEntry& option)
{
    std::vector<AppearanceOption>* target = this->getOptionsForCollection(collection);
    if (target == nullptr)
    {
        return;
    }

    target->push_back(option);
    this->syncOptionCollectionState(collection);

    if (this->resourcesLoaded)
    {
        this->loadAppearanceIcons();
    }
}

void AccountManagementWidget::clearOptionsForCollection(OptionCollectionType collection)
{
    std::vector<AppearanceOption>* target = this->getOptionsForCollection(collection);
    if (target == nullptr)
    {
        return;
    }

    target->clear();
    this->syncOptionCollectionState(collection);

    if (this->resourcesLoaded)
    {
        this->loadAppearanceIcons();
    }
}

void AccountManagementWidget::setEliteProgressData(const EliteProgressData& progressData)
{
    this->eliteProgressData.currentPoints = (std::max)(0, progressData.currentPoints);
    this->eliteProgressData.hasNextShip = progressData.hasNextShip;

    if (this->eliteProgressData.hasNextShip)
    {
        this->eliteProgressData.nextShipPointsRequired =
            (std::max)(this->eliteProgressData.currentPoints, progressData.nextShipPointsRequired);
        return;
    }

    this->eliteProgressData.nextShipPointsRequired = 0;
}

void AccountManagementWidget::setElitePointsCurrent(int points)
{
    this->eliteProgressData.currentPoints = (std::max)(0, points);
    if (this->eliteProgressData.hasNextShip)
    {
        this->eliteProgressData.nextShipPointsRequired =
            (std::max)(this->eliteProgressData.currentPoints, this->eliteProgressData.nextShipPointsRequired);
    }
}

void AccountManagementWidget::setPlayerIdentifier(const std::string& value)
{
    this->playerIdentifier = value;
}

void AccountManagementWidget::setPirateSince(const std::string& value)
{
    this->pirateSinceText = value;
}

void AccountManagementWidget::setPlayerLevel(int level)
{
    this->playerLevel = (std::max)(0, level);
}

void AccountManagementWidget::setExperiencePointsCurrent(int points)
{
    this->experiencePointsCurrent = (std::max)(0, points);
}

void AccountManagementWidget::setCombatPointsCurrent(int points)
{
    this->combatPointsCurrent = (std::max)(0, points);
}

void AccountManagementWidget::setPremiumSince(const std::string& value)
{
    this->premiumSinceText = value;
}

void AccountManagementWidget::setProfileName(const std::string& value)
{
    this->profileName = clampProfileNameToUiLimit(value);
    this->profileCursorIndex = this->profileName.size();
}

AccountManagementWidget::EliteProgressData AccountManagementWidget::getEliteProgress(void) const
{
    EliteProgressData data = this->eliteProgressData;
    data.currentPoints = (std::max)(0, data.currentPoints);
    if (data.hasNextShip)
    {
        data.nextShipPointsRequired = (std::max)(data.currentPoints, data.nextShipPointsRequired);
    }
    else
    {
        data.nextShipPointsRequired = 0;
    }
    return data;
}

void AccountManagementWidget::clearIcons(std::vector<RC2D_Image>& icons)
{
    for (RC2D_Image& icon : icons)
    {
        ResetStorageImageRef(&icon);
    }
    icons.clear();
}

void AccountManagementWidget::clearAllData(void)
{
    this->clearIcons(this->eliteShipIcons);
    this->clearIcons(this->specialShipIcons);
    this->clearIcons(this->shipOptionIcons);
    this->clearIcons(this->coatingOptionIcons);
    this->clearIcons(this->repairStyleOptionIcons);
    this->clearIcons(this->speedStyleOptionIcons);
    this->clearIcons(this->explosionStyleOptionIcons);
    this->clearIcons(this->rocketStyleOptionIcons);
    this->clearIcons(this->projectileStyleOptionIcons);
    this->clearIcons(this->moveClickStyleOptionIcons);
    this->clearIcons(this->emoteOptionIcons);
    this->clearIcons(this->storageEquipmentOptionIcons);

    this->eliteShips.clear();
    this->specialShips.clear();
    this->shipOptions.clear();
    this->coatingOptions.clear();
    this->repairStyleOptions.clear();
    this->speedStyleOptions.clear();
    this->explosionStyleOptions.clear();
    this->rocketStyleOptions.clear();
    this->projectileStyleOptions.clear();
    this->moveClickStyleOptions.clear();
    this->emoteOptions.clear();
    this->storageEquipmentOptions.clear();

    this->playerIdentifier.clear();
    this->pirateSinceText.clear();
    this->playerLevel = 0;
    this->experiencePointsCurrent = 0;
    this->eliteProgressData = EliteProgressData{0, 0, false};
    this->combatPointsCurrent = 0;
    this->premiumSinceText.clear();
    this->selectedEliteShip = 0;
    this->selectedSpecialShip = 0;
    this->eliteFirstRow = 0;
    this->specialFirstRow = 0;
    this->selectedShipOption = 0;
    this->selectedCoatingOption = 0;
    this->selectedRepairStyleOption = 0;
    this->selectedSpeedStyleOption = 0;
    this->selectedExplosionStyleOption = 0;
    this->selectedRocketStyleOption = 0;
    this->selectedProjectileStyleOption = 0;
    this->selectedMoveClickStyleOption = 0;
    this->selectedEmoteOption = 0;
    this->selectedStorageEquipmentOption = 0;
    this->profileName.clear();
    this->profileCursorIndex = 0;
}

std::vector<AccountManagementWidget::ShipEntry>* AccountManagementWidget::getShipsForCollection(
    ShipCollectionType collection)
{
    switch (collection)
    {
        case ShipCollectionType::ELITE_ACQUIRED:
            return &this->eliteShips;
        case ShipCollectionType::SPECIAL_ACQUIRED:
            return &this->specialShips;
        default:
            break;
    }
    return nullptr;
}

const std::vector<AccountManagementWidget::ShipEntry>* AccountManagementWidget::getShipsForCollection(
    ShipCollectionType collection) const
{
    switch (collection)
    {
        case ShipCollectionType::ELITE_ACQUIRED:
            return &this->eliteShips;
        case ShipCollectionType::SPECIAL_ACQUIRED:
            return &this->specialShips;
        default:
            break;
    }
    return nullptr;
}

std::vector<AccountManagementWidget::AppearanceOption>* AccountManagementWidget::getOptionsForCollection(
    OptionCollectionType collection)
{
    switch (collection)
    {
        case OptionCollectionType::SHIP_BONUS:
            return &this->shipOptions;
        case OptionCollectionType::SHIP_STYLE:
            return &this->coatingOptions;
        case OptionCollectionType::REPAIR_STYLE:
            return &this->repairStyleOptions;
        case OptionCollectionType::SPEED_STYLE:
            return &this->speedStyleOptions;
        case OptionCollectionType::PROJECTILE_IMPACT_STYLE:
            return &this->explosionStyleOptions;
        case OptionCollectionType::ROCKET_STYLE:
            return &this->rocketStyleOptions;
        case OptionCollectionType::PROJECTILE_STYLE:
            return &this->projectileStyleOptions;
        case OptionCollectionType::MOVE_CLICK_STYLE:
            return &this->moveClickStyleOptions;
        case OptionCollectionType::EMOTE:
            return &this->emoteOptions;
        case OptionCollectionType::STORAGE_EQUIPMENT:
            return &this->storageEquipmentOptions;
        default:
            break;
    }
    return nullptr;
}

const std::vector<AccountManagementWidget::AppearanceOption>* AccountManagementWidget::getOptionsForCollection(
    OptionCollectionType collection) const
{
    switch (collection)
    {
        case OptionCollectionType::SHIP_BONUS:
            return &this->shipOptions;
        case OptionCollectionType::SHIP_STYLE:
            return &this->coatingOptions;
        case OptionCollectionType::REPAIR_STYLE:
            return &this->repairStyleOptions;
        case OptionCollectionType::SPEED_STYLE:
            return &this->speedStyleOptions;
        case OptionCollectionType::PROJECTILE_IMPACT_STYLE:
            return &this->explosionStyleOptions;
        case OptionCollectionType::ROCKET_STYLE:
            return &this->rocketStyleOptions;
        case OptionCollectionType::PROJECTILE_STYLE:
            return &this->projectileStyleOptions;
        case OptionCollectionType::MOVE_CLICK_STYLE:
            return &this->moveClickStyleOptions;
        case OptionCollectionType::EMOTE:
            return &this->emoteOptions;
        case OptionCollectionType::STORAGE_EQUIPMENT:
            return &this->storageEquipmentOptions;
        default:
            break;
    }
    return nullptr;
}

int* AccountManagementWidget::getSelectedIndexForCollection(OptionCollectionType collection)
{
    switch (collection)
    {
        case OptionCollectionType::SHIP_BONUS:
            return &this->selectedShipOption;
        case OptionCollectionType::SHIP_STYLE:
            return &this->selectedCoatingOption;
        case OptionCollectionType::REPAIR_STYLE:
            return &this->selectedRepairStyleOption;
        case OptionCollectionType::SPEED_STYLE:
            return &this->selectedSpeedStyleOption;
        case OptionCollectionType::PROJECTILE_IMPACT_STYLE:
            return &this->selectedExplosionStyleOption;
        case OptionCollectionType::ROCKET_STYLE:
            return &this->selectedRocketStyleOption;
        case OptionCollectionType::PROJECTILE_STYLE:
            return &this->selectedProjectileStyleOption;
        case OptionCollectionType::MOVE_CLICK_STYLE:
            return &this->selectedMoveClickStyleOption;
        case OptionCollectionType::EMOTE:
            return &this->selectedEmoteOption;
        case OptionCollectionType::STORAGE_EQUIPMENT:
            return &this->selectedStorageEquipmentOption;
        default:
            break;
    }
    return nullptr;
}

void AccountManagementWidget::syncShipCollectionState(ShipCollectionType collection)
{
    std::vector<ShipEntry>* ships = this->getShipsForCollection(collection);
    if (ships == nullptr)
    {
        return;
    }

    switch (collection)
    {
        case ShipCollectionType::ELITE_ACQUIRED:
            this->selectedEliteShip = ships->empty()
                ? 0
                : (std::max)(0, (std::min)(this->selectedEliteShip, static_cast<int>(ships->size()) - 1));
            this->eliteFirstRow = 0;
            break;
        case ShipCollectionType::SPECIAL_ACQUIRED:
            this->selectedSpecialShip = ships->empty()
                ? 0
                : (std::max)(0, (std::min)(this->selectedSpecialShip, static_cast<int>(ships->size()) - 1));
            this->specialFirstRow = 0;
            break;
        default:
            break;
    }
}

void AccountManagementWidget::syncOptionCollectionState(OptionCollectionType collection)
{
    std::vector<AppearanceOption>* options = this->getOptionsForCollection(collection);
    int* selectedIndex = this->getSelectedIndexForCollection(collection);
    if (options == nullptr || selectedIndex == nullptr)
    {
        return;
    }

    *selectedIndex = options->empty()
        ? 0
        : (std::max)(0, (std::min)(*selectedIndex, static_cast<int>(options->size()) - 1));

    if (this->openPicker != AppearancePickerType::NONE &&
        this->getCollectionForPicker(this->openPicker) == collection &&
        options->empty())
    {
        this->closeAppearancePicker();
    }
}

AccountManagementWidget::OptionCollectionType AccountManagementWidget::getCollectionForPicker(
    AppearancePickerType picker) const
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return OptionCollectionType::SHIP_BONUS;
        case AppearancePickerType::SHIP_STYLE:
            return OptionCollectionType::SHIP_STYLE;
        case AppearancePickerType::REPAIR_STYLE:
            return OptionCollectionType::REPAIR_STYLE;
        case AppearancePickerType::SPEED_STYLE:
            return OptionCollectionType::SPEED_STYLE;
        case AppearancePickerType::EXPLOSION_STYLE:
            return OptionCollectionType::PROJECTILE_IMPACT_STYLE;
        case AppearancePickerType::ROCKET_STYLE:
            return OptionCollectionType::ROCKET_STYLE;
        case AppearancePickerType::PROJECTILE_STYLE:
            return OptionCollectionType::PROJECTILE_STYLE;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return OptionCollectionType::MOVE_CLICK_STYLE;
        case AppearancePickerType::EMOTE:
            return OptionCollectionType::EMOTE;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
        case AppearancePickerType::NONE:
        default:
            return OptionCollectionType::STORAGE_EQUIPMENT;
    }
}

int AccountManagementWidget::getVisibleShipCountForTab(ActiveTab tab) const
{
    const std::vector<ShipEntry>* ships = this->getShipsForTab(tab);
    if (ships == nullptr)
    {
        return 0;
    }

    return static_cast<int>(ships->size());
}

int AccountManagementWidget::getVisibleShipSourceIndexForTab(ActiveTab tab, int visibleIndex) const
{
    if (visibleIndex < 0)
    {
        return -1;
    }

    const std::vector<ShipEntry>* ships = this->getShipsForTab(tab);
    if (ships == nullptr)
    {
        return -1;
    }

    return visibleIndex < static_cast<int>(ships->size()) ? visibleIndex : -1;
}

void AccountManagementWidget::loadShipIcons(void)
{
    this->clearIcons(this->eliteShipIcons);
    this->clearIcons(this->specialShipIcons);

    this->eliteShipIcons.reserve(this->eliteShips.size());
    for (const ShipEntry& entry : this->eliteShips)
    {
        this->eliteShipIcons.push_back(loadImageFromTitleOrEmpty(entry.previewAssetPath));
    }

    this->specialShipIcons.reserve(this->specialShips.size());
    for (const ShipEntry& entry : this->specialShips)
    {
        this->specialShipIcons.push_back(loadImageFromTitleOrEmpty(entry.previewAssetPath));
    }
}

void AccountManagementWidget::loadAppearanceIcons(void)
{
    this->clearIcons(this->shipOptionIcons);
    this->clearIcons(this->coatingOptionIcons);
    this->clearIcons(this->repairStyleOptionIcons);
    this->clearIcons(this->speedStyleOptionIcons);
    this->clearIcons(this->explosionStyleOptionIcons);
    this->clearIcons(this->rocketStyleOptionIcons);
    this->clearIcons(this->projectileStyleOptionIcons);
    this->clearIcons(this->moveClickStyleOptionIcons);
    this->clearIcons(this->emoteOptionIcons);
    this->clearIcons(this->storageEquipmentOptionIcons);

    this->shipOptionIcons.reserve(this->shipOptions.size());
    for (const AppearanceOption& option : this->shipOptions)
    {
        this->shipOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->coatingOptionIcons.reserve(this->coatingOptions.size());
    for (const AppearanceOption& option : this->coatingOptions)
    {
        this->coatingOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->repairStyleOptionIcons.reserve(this->repairStyleOptions.size());
    for (const AppearanceOption& option : this->repairStyleOptions)
    {
        this->repairStyleOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->speedStyleOptionIcons.reserve(this->speedStyleOptions.size());
    for (const AppearanceOption& option : this->speedStyleOptions)
    {
        this->speedStyleOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->explosionStyleOptionIcons.reserve(this->explosionStyleOptions.size());
    for (const AppearanceOption& option : this->explosionStyleOptions)
    {
        this->explosionStyleOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->rocketStyleOptionIcons.reserve(this->rocketStyleOptions.size());
    for (const AppearanceOption& option : this->rocketStyleOptions)
    {
        this->rocketStyleOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->projectileStyleOptionIcons.reserve(this->projectileStyleOptions.size());
    for (const AppearanceOption& option : this->projectileStyleOptions)
    {
        this->projectileStyleOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->moveClickStyleOptionIcons.reserve(this->moveClickStyleOptions.size());
    for (const AppearanceOption& option : this->moveClickStyleOptions)
    {
        this->moveClickStyleOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->emoteOptionIcons.reserve(this->emoteOptions.size());
    for (const AppearanceOption& option : this->emoteOptions)
    {
        this->emoteOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }

    this->storageEquipmentOptionIcons.reserve(this->storageEquipmentOptions.size());
    for (const AppearanceOption& option : this->storageEquipmentOptions)
    {
        this->storageEquipmentOptionIcons.push_back(loadImageFromTitleOrEmpty(option.previewAssetPath));
    }
}

std::vector<AccountManagementWidget::ShipEntry>* AccountManagementWidget::getShipsForTab(ActiveTab tab)
{
    switch (tab)
    {
        case ActiveTab::ELITE_SHIPS:
            return &this->eliteShips;
        case ActiveTab::SPECIAL_SHIPS:
            return &this->specialShips;
        default:
            break;
    }
    return nullptr;
}

const std::vector<AccountManagementWidget::ShipEntry>* AccountManagementWidget::getShipsForTab(ActiveTab tab) const
{
    switch (tab)
    {
        case ActiveTab::ELITE_SHIPS:
            return &this->eliteShips;
        case ActiveTab::SPECIAL_SHIPS:
            return &this->specialShips;
        default:
            break;
    }
    return nullptr;
}

std::vector<RC2D_Image>* AccountManagementWidget::getShipIconsForTab(ActiveTab tab)
{
    switch (tab)
    {
        case ActiveTab::ELITE_SHIPS:
            return &this->eliteShipIcons;
        case ActiveTab::SPECIAL_SHIPS:
            return &this->specialShipIcons;
        default:
            break;
    }
    return nullptr;
}

const std::vector<RC2D_Image>* AccountManagementWidget::getShipIconsForTab(ActiveTab tab) const
{
    switch (tab)
    {
        case ActiveTab::ELITE_SHIPS:
            return &this->eliteShipIcons;
        case ActiveTab::SPECIAL_SHIPS:
            return &this->specialShipIcons;
        default:
            break;
    }
    return nullptr;
}

int* AccountManagementWidget::getFirstRowForTab(ActiveTab tab)
{
    switch (tab)
    {
        case ActiveTab::ELITE_SHIPS:
            return &this->eliteFirstRow;
        case ActiveTab::SPECIAL_SHIPS:
            return &this->specialFirstRow;
        default:
            break;
    }
    return nullptr;
}

const int* AccountManagementWidget::getFirstRowForTab(ActiveTab tab) const
{
    switch (tab)
    {
        case ActiveTab::ELITE_SHIPS:
            return &this->eliteFirstRow;
        case ActiveTab::SPECIAL_SHIPS:
            return &this->specialFirstRow;
        default:
            break;
    }
    return nullptr;
}

std::vector<AccountManagementWidget::AppearanceOption>* AccountManagementWidget::getOptionsForPicker(AppearancePickerType picker)
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return &this->shipOptions;
        case AppearancePickerType::SHIP_STYLE:
            return &this->coatingOptions;
        case AppearancePickerType::REPAIR_STYLE:
            return &this->repairStyleOptions;
        case AppearancePickerType::SPEED_STYLE:
            return &this->speedStyleOptions;
        case AppearancePickerType::EXPLOSION_STYLE:
            return &this->explosionStyleOptions;
        case AppearancePickerType::ROCKET_STYLE:
            return &this->rocketStyleOptions;
        case AppearancePickerType::PROJECTILE_STYLE:
            return &this->projectileStyleOptions;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return &this->moveClickStyleOptions;
        case AppearancePickerType::EMOTE:
            return &this->emoteOptions;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
            return &this->storageEquipmentOptions;
        default:
            break;
    }
    return nullptr;
}

const std::vector<AccountManagementWidget::AppearanceOption>* AccountManagementWidget::getOptionsForPicker(AppearancePickerType picker) const
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return &this->shipOptions;
        case AppearancePickerType::SHIP_STYLE:
            return &this->coatingOptions;
        case AppearancePickerType::REPAIR_STYLE:
            return &this->repairStyleOptions;
        case AppearancePickerType::SPEED_STYLE:
            return &this->speedStyleOptions;
        case AppearancePickerType::EXPLOSION_STYLE:
            return &this->explosionStyleOptions;
        case AppearancePickerType::ROCKET_STYLE:
            return &this->rocketStyleOptions;
        case AppearancePickerType::PROJECTILE_STYLE:
            return &this->projectileStyleOptions;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return &this->moveClickStyleOptions;
        case AppearancePickerType::EMOTE:
            return &this->emoteOptions;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
            return &this->storageEquipmentOptions;
        default:
            break;
    }
    return nullptr;
}

std::vector<RC2D_Image>* AccountManagementWidget::getIconsForPicker(AppearancePickerType picker)
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return &this->shipOptionIcons;
        case AppearancePickerType::SHIP_STYLE:
            return &this->coatingOptionIcons;
        case AppearancePickerType::REPAIR_STYLE:
            return &this->repairStyleOptionIcons;
        case AppearancePickerType::SPEED_STYLE:
            return &this->speedStyleOptionIcons;
        case AppearancePickerType::EXPLOSION_STYLE:
            return &this->explosionStyleOptionIcons;
        case AppearancePickerType::ROCKET_STYLE:
            return &this->rocketStyleOptionIcons;
        case AppearancePickerType::PROJECTILE_STYLE:
            return &this->projectileStyleOptionIcons;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return &this->moveClickStyleOptionIcons;
        case AppearancePickerType::EMOTE:
            return &this->emoteOptionIcons;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
            return &this->storageEquipmentOptionIcons;
        default:
            break;
    }
    return nullptr;
}

const std::vector<RC2D_Image>* AccountManagementWidget::getIconsForPicker(AppearancePickerType picker) const
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return &this->shipOptionIcons;
        case AppearancePickerType::SHIP_STYLE:
            return &this->coatingOptionIcons;
        case AppearancePickerType::REPAIR_STYLE:
            return &this->repairStyleOptionIcons;
        case AppearancePickerType::SPEED_STYLE:
            return &this->speedStyleOptionIcons;
        case AppearancePickerType::EXPLOSION_STYLE:
            return &this->explosionStyleOptionIcons;
        case AppearancePickerType::ROCKET_STYLE:
            return &this->rocketStyleOptionIcons;
        case AppearancePickerType::PROJECTILE_STYLE:
            return &this->projectileStyleOptionIcons;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return &this->moveClickStyleOptionIcons;
        case AppearancePickerType::EMOTE:
            return &this->emoteOptionIcons;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
            return &this->storageEquipmentOptionIcons;
        default:
            break;
    }
    return nullptr;
}

int* AccountManagementWidget::getSelectedIndexForPicker(AppearancePickerType picker)
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return &this->selectedShipOption;
        case AppearancePickerType::SHIP_STYLE:
            return &this->selectedCoatingOption;
        case AppearancePickerType::REPAIR_STYLE:
            return &this->selectedRepairStyleOption;
        case AppearancePickerType::SPEED_STYLE:
            return &this->selectedSpeedStyleOption;
        case AppearancePickerType::EXPLOSION_STYLE:
            return &this->selectedExplosionStyleOption;
        case AppearancePickerType::ROCKET_STYLE:
            return &this->selectedRocketStyleOption;
        case AppearancePickerType::PROJECTILE_STYLE:
            return &this->selectedProjectileStyleOption;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return &this->selectedMoveClickStyleOption;
        case AppearancePickerType::EMOTE:
            return &this->selectedEmoteOption;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
            return &this->selectedStorageEquipmentOption;
        default:
            break;
    }
    return nullptr;
}

const int* AccountManagementWidget::getSelectedIndexForPicker(AppearancePickerType picker) const
{
    switch (picker)
    {
        case AppearancePickerType::SHIP_BONUS:
            return &this->selectedShipOption;
        case AppearancePickerType::SHIP_STYLE:
            return &this->selectedCoatingOption;
        case AppearancePickerType::REPAIR_STYLE:
            return &this->selectedRepairStyleOption;
        case AppearancePickerType::SPEED_STYLE:
            return &this->selectedSpeedStyleOption;
        case AppearancePickerType::EXPLOSION_STYLE:
            return &this->selectedExplosionStyleOption;
        case AppearancePickerType::ROCKET_STYLE:
            return &this->selectedRocketStyleOption;
        case AppearancePickerType::PROJECTILE_STYLE:
            return &this->selectedProjectileStyleOption;
        case AppearancePickerType::MOVE_CLICK_STYLE:
            return &this->selectedMoveClickStyleOption;
        case AppearancePickerType::EMOTE:
            return &this->selectedEmoteOption;
        case AppearancePickerType::STORAGE_ITEM_LEFT:
        case AppearancePickerType::STORAGE_ITEM_RIGHT:
            return &this->selectedStorageEquipmentOption;
        default:
            break;
    }
    return nullptr;
}

void AccountManagementWidget::openAppearancePickerForRect(AppearancePickerType picker, const SDL_FRect& anchorRect)
{
    this->openPicker = picker;
    this->openPickerAnchorRect = anchorRect;
    this->pickerFirstRow = 0;
    this->pickerScrollDragging = false;
}

void AccountManagementWidget::closeAppearancePicker(void)
{
    this->openPicker = AppearancePickerType::NONE;
    this->openPickerAnchorRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->pickerFirstRow = 0;
    this->pickerScrollDragging = false;
    this->pickerScrollDragOffsetY = 0.0f;
    this->pickerScrollWheelHighlightSec = 0.0f;
}

void AccountManagementWidget::clearProfileInputFocus(void)
{
    this->profileInputFocused = false;
    this->profileCursorVisible = false;
    this->profileCursorBlinkElapsed = 0.0;
}

void AccountManagementWidget::clearAllFocus(void)
{
    this->clearProfileInputFocus();
    this->closeAppearancePicker();
}

void AccountManagementWidget::load(void)
{
    this->titleFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 20.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Semibold.ttf", RC2D_STORAGE_TITLE, 14.0f);
    this->smallFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 13.0f);
    this->controlIcons.load();
    this->resourcesLoaded = true;

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetOffsetX = 0.0f;
    this->widgetOffsetY = 0.0f;
    this->widgetRect = SDL_FRect{baseRect.x, baseRect.y, baseRect.w, baseRect.h};

    this->visible = false;
    this->cursorEnabled = true;
    this->activeTab = ActiveTab::ACCOUNT;

    this->widgetDragging = false;
    this->widgetDragOffsetX = 0.0f;
    this->widgetDragOffsetY = 0.0f;
    this->fleetScrollDragging = false;
    this->fleetScrollDragOffsetY = 0.0f;
    this->fleetScrollWheelHighlightSec = 0.0f;

    this->profileInputFocused = false;
    this->profileCursorIndex = this->profileName.size();
    this->profileCursorVisible = false;
    this->profileCursorBlinkElapsed = 0.0;
    this->loadShipIcons();
    this->loadAppearanceIcons();

    this->closeAppearancePicker();
}

void AccountManagementWidget::unload(void)
{
    this->clearIcons(this->eliteShipIcons);
    this->clearIcons(this->specialShipIcons);
    this->clearIcons(this->shipOptionIcons);
    this->clearIcons(this->coatingOptionIcons);
    this->clearIcons(this->repairStyleOptionIcons);
    this->clearIcons(this->speedStyleOptionIcons);
    this->clearIcons(this->explosionStyleOptionIcons);
    this->clearIcons(this->rocketStyleOptionIcons);
    this->clearIcons(this->projectileStyleOptionIcons);
    this->clearIcons(this->moveClickStyleOptionIcons);
    this->clearIcons(this->emoteOptionIcons);
    this->clearIcons(this->storageEquipmentOptionIcons);
    this->controlIcons.unload();
    this->resourcesLoaded = false;
    ResetStorageFontRef(&this->smallFont);
    ResetStorageFontRef(&this->bodyFont);
    ResetStorageFontRef(&this->titleFont);
}

bool AccountManagementWidget::containsPoint(float x, float y) const
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

HudCursorType AccountManagementWidget::getDesiredCursor(float x, float y) const
{
    if (!this->visible)
    {
        return HudCursorType::NONE;
    }

    if (this->pickerScrollDragging || this->fleetScrollDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const WidgetLayout layout = buildLayout(currentRect);

    if (this->openPicker != AppearancePickerType::NONE)
    {
        const std::vector<AppearanceOption>* options = this->getOptionsForPicker(this->openPicker);
        const int totalRows = options != nullptr ? static_cast<int>(options->size()) : 0;
        int preferredPickerRows = kPickerMaxVisibleRows;
        switch (this->openPicker)
        {
            case AppearancePickerType::REPAIR_STYLE:
            case AppearancePickerType::SPEED_STYLE:
            case AppearancePickerType::EXPLOSION_STYLE:
            case AppearancePickerType::ROCKET_STYLE:
            case AppearancePickerType::PROJECTILE_STYLE:
            case AppearancePickerType::MOVE_CLICK_STYLE:
                preferredPickerRows = kEffectPickerMaxVisibleRows;
                break;
            default:
                break;
        }

        const bool openUpward = this->openPicker == AppearancePickerType::EMOTE;
        const PickerLayout pickerLayout = buildPickerLayout(
            currentRect,
            this->openPickerAnchorRect,
            totalRows,
            preferredPickerRows,
            openUpward);

        if (isPointInRect(x, y, pickerLayout.panel))
        {
            if (pickerLayout.maxFirstRow > 0 && isPointInRect(x, y, pickerLayout.scrollTrack))
            {
                return HudCursorType::RESIZE_VERTICAL;
            }
            return HudCursorType::POINTER;
        }
    }

    if (!isPointInRect(x, y, currentRect))
    {
        return HudCursorType::NONE;
    }

    if (isPointInRect(x, y, layout.closeButton) ||
        isPointInRect(x, y, layout.tabAccount) ||
        isPointInRect(x, y, layout.tabAppearance) ||
        isPointInRect(x, y, layout.tabShipManagement) ||
        isPointInRect(x, y, layout.tabElite) ||
        isPointInRect(x, y, layout.tabSpecial) ||
        isPointInRect(x, y, layout.tabStorageEquipped))
    {
        return HudCursorType::POINTER;
    }

    if (this->activeTab == ActiveTab::ACCOUNT)
    {
        if (isPointInRect(x, y, layout.accountProfileNameInput))
        {
            return HudCursorType::TEXT;
        }
        if (isPointInRect(x, y, layout.accountProfileApplyButton))
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == ActiveTab::APPEARANCE)
    {
        if (isPointInRect(x, y, layout.appearanceCoatingCase) ||
            isPointInRect(x, y, layout.appearanceRepairCase) ||
            isPointInRect(x, y, layout.appearanceSpeedCase) ||
            isPointInRect(x, y, layout.appearanceExplosionCase) ||
            isPointInRect(x, y, layout.appearanceRocketCase) ||
            isPointInRect(x, y, layout.appearanceProjectileCase) ||
            isPointInRect(x, y, layout.appearanceMoveClickCase) ||
            isPointInRect(x, y, layout.appearanceEmotesCase))
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == ActiveTab::SHIP_MANAGEMENT)
    {
        if (isPointInRect(x, y, layout.shipManagementShipCase))
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == ActiveTab::STORAGE_EQUIPPED)
    {
        if (isPointInRect(x, y, layout.storageLeftDropdown) ||
            isPointInRect(x, y, layout.storageMiddleDropdown))
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == ActiveTab::ELITE_SHIPS || this->activeTab == ActiveTab::SPECIAL_SHIPS)
    {
        const std::vector<ShipEntry>* ships = this->getShipsForTab(this->activeTab);
        if (ships != nullptr)
        {
            const SDL_FRect viewport = (this->activeTab == ActiveTab::ELITE_SHIPS)
                ? layout.fleetGridViewportElite
                : layout.fleetGridViewportSpecial;
            const FleetMetrics metrics = buildFleetMetrics(viewport, this->getVisibleShipCountForTab(this->activeTab));
            if (metrics.maxFirstRow > 0 && isPointInRect(x, y, metrics.scrollTrack))
            {
                return HudCursorType::RESIZE_VERTICAL;
            }
        }
    }

    if (isPointInRect(x, y, layout.headerDragRect))
    {
        return HudCursorType::MOVE;
    }

    return HudCursorType::DEFAULT;
}

void AccountManagementWidget::clearFocus(void)
{
    this->clearAllFocus();
}

void AccountManagementWidget::openShipManagement(void)
{
    this->visible = true;
    this->activeTab = ActiveTab::SHIP_MANAGEMENT;
    this->widgetDragging = false;
    this->fleetScrollDragging = false;
    this->pickerScrollDragging = false;
    this->clearFocus();
    this->closeAppearancePicker();
}

void AccountManagementWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->fleetScrollDragging = false;
    this->pickerScrollDragging = false;
    this->fleetScrollWheelHighlightSec = 0.0f;
    this->clearFocus();
    this->closeAppearancePicker();
}

void AccountManagementWidget::update(double dt)
{
    const float dtF = static_cast<float>(dt);
    if (this->fleetScrollWheelHighlightSec > 0.0f)
    {
        this->fleetScrollWheelHighlightSec -= dtF;
        if (this->fleetScrollWheelHighlightSec < 0.0f)
        {
            this->fleetScrollWheelHighlightSec = 0.0f;
        }
    }
    if (this->pickerScrollWheelHighlightSec > 0.0f)
    {
        this->pickerScrollWheelHighlightSec -= dtF;
        if (this->pickerScrollWheelHighlightSec < 0.0f)
        {
            this->pickerScrollWheelHighlightSec = 0.0f;
        }
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    this->widgetRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };

    if (this->profileInputFocused)
    {
        this->profileCursorBlinkElapsed += dt;
        while (this->profileCursorBlinkElapsed >= kCursorBlinkPeriod)
        {
            this->profileCursorBlinkElapsed -= kCursorBlinkPeriod;
            this->profileCursorVisible = !this->profileCursorVisible;
        }
    }
    else
    {
        this->profileCursorBlinkElapsed = 0.0;
        this->profileCursorVisible = false;
    }

    const WidgetLayout layout = buildLayout(this->widgetRect);

    if (!this->widgetDragging && !this->fleetScrollDragging && !this->pickerScrollDragging)
    {
        return;
    }

    if (!rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->widgetDragging = false;
        this->fleetScrollDragging = false;
        this->pickerScrollDragging = false;
        return;
    }

    if (this->pickerScrollDragging && this->openPicker != AppearancePickerType::NONE)
    {
        const std::vector<AppearanceOption>* options = this->getOptionsForPicker(this->openPicker);
        const int totalRows = options != nullptr ? static_cast<int>(options->size()) : 0;
        int preferredPickerRows = kPickerMaxVisibleRows;
        switch (this->openPicker)
        {
            case AppearancePickerType::REPAIR_STYLE:
            case AppearancePickerType::SPEED_STYLE:
            case AppearancePickerType::EXPLOSION_STYLE:
            case AppearancePickerType::ROCKET_STYLE:
            case AppearancePickerType::PROJECTILE_STYLE:
            case AppearancePickerType::MOVE_CLICK_STYLE:
                preferredPickerRows = kEffectPickerMaxVisibleRows;
                break;
            default:
                break;
        }

        const bool openUpward = this->openPicker == AppearancePickerType::EMOTE;
        const PickerLayout pickerLayout = buildPickerLayout(
            this->widgetRect,
            this->openPickerAnchorRect,
            totalRows,
            preferredPickerRows,
            openUpward);

        if (pickerLayout.maxFirstRow <= 0)
        {
            this->pickerFirstRow = 0;
            this->pickerScrollDragging = false;
        }
        else
        {
            const float thumbHeight = (std::max)(
                kMinThumbHeight,
                (pickerLayout.scrollTrack.h * static_cast<float>(pickerLayout.visibleRows) / static_cast<float>((std::max)(1, pickerLayout.totalRows))));
            const float thumbTravel = (std::max)(1.0f, pickerLayout.scrollTrack.h - thumbHeight);

            float mx = 0.0f;
            float my = 0.0f;
            getMouseRenderPosition(&mx, &my);
            (void)mx;

            const float thumbTop = clampf(my - this->pickerScrollDragOffsetY, pickerLayout.scrollTrack.y, pickerLayout.scrollTrack.y + thumbTravel);
            const float t = (thumbTop - pickerLayout.scrollTrack.y) / thumbTravel;
            this->pickerFirstRow = static_cast<int>(t * static_cast<float>(pickerLayout.maxFirstRow) + 0.5f);
            this->pickerFirstRow = (std::max)(0, (std::min)(this->pickerFirstRow, pickerLayout.maxFirstRow));
        }
        return;
    }

    if (this->fleetScrollDragging)
    {
        std::vector<ShipEntry>* ships = this->getShipsForTab(this->activeTab);
        int* firstRowPtr = this->getFirstRowForTab(this->activeTab);
        if (ships == nullptr || firstRowPtr == nullptr)
        {
            this->fleetScrollDragging = false;
            return;
        }

        const SDL_FRect viewport = (this->activeTab == ActiveTab::ELITE_SHIPS)
            ? layout.fleetGridViewportElite
            : layout.fleetGridViewportSpecial;
        const FleetMetrics metrics = buildFleetMetrics(viewport, this->getVisibleShipCountForTab(this->activeTab));
        int& firstRow = *firstRowPtr;

        if (metrics.maxFirstRow <= 0)
        {
            firstRow = 0;
            this->fleetScrollDragging = false;
            return;
        }

        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (metrics.scrollTrack.h * static_cast<float>(metrics.visibleRows) / static_cast<float>((std::max)(1, metrics.totalRows))));
        const float thumbTravel = (std::max)(1.0f, metrics.scrollTrack.h - thumbHeight);

        float mx = 0.0f;
        float my = 0.0f;
        getMouseRenderPosition(&mx, &my);
        (void)mx;

        const float thumbTop = clampf(my - this->fleetScrollDragOffsetY, metrics.scrollTrack.y, metrics.scrollTrack.y + thumbTravel);
        const float t = (thumbTop - metrics.scrollTrack.y) / thumbTravel;
        firstRow = static_cast<int>(t * static_cast<float>(metrics.maxFirstRow) + 0.5f);
        firstRow = (std::max)(0, (std::min)(firstRow, metrics.maxFirstRow));
        return;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    getMouseRenderPosition(&mouseX, &mouseY);
    this->widgetOffsetX = (mouseX - this->widgetDragOffsetX) - baseRect.x;
    this->widgetOffsetY = (mouseY - this->widgetDragOffsetY) - baseRect.y;
    this->widgetRect.x = baseRect.x + this->widgetOffsetX;
    this->widgetRect.y = baseRect.y + this->widgetOffsetY;
}

bool AccountManagementWidget::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
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

    if (!isPointInRect(x, y, this->widgetRect))
    {
        return false;
    }

    const WidgetLayout layout = buildLayout(this->widgetRect);

    if (isPointInRect(x, y, layout.closeButton))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->fleetScrollDragging = false;
        this->pickerScrollDragging = false;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabAccount))
    {
        this->activeTab = ActiveTab::ACCOUNT;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabAppearance))
    {
        this->activeTab = ActiveTab::APPEARANCE;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabShipManagement))
    {
        this->activeTab = ActiveTab::SHIP_MANAGEMENT;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabElite))
    {
        this->activeTab = ActiveTab::ELITE_SHIPS;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabSpecial))
    {
        this->activeTab = ActiveTab::SPECIAL_SHIPS;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabStorageEquipped))
    {
        this->activeTab = ActiveTab::STORAGE_EQUIPPED;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.headerDragRect))
    {
        this->widgetDragging = true;
        this->fleetScrollDragging = false;
        this->pickerScrollDragging = false;
        this->widgetDragOffsetX = x - this->widgetRect.x;
        this->widgetDragOffsetY = y - this->widgetRect.y;
        this->clearAllFocus();
        return true;
    }

    // Re-clic sur l'ancre de liste: ferme la liste meme si le panel se superpose.
    if (this->openPicker != AppearancePickerType::NONE && isPointInRect(x, y, this->openPickerAnchorRect))
    {
        this->closeAppearancePicker();
        return true;
    }

    if (this->openPicker != AppearancePickerType::NONE)
    {
        std::vector<AppearanceOption>* options = this->getOptionsForPicker(this->openPicker);
        int* selectedIndex = this->getSelectedIndexForPicker(this->openPicker);

        const int totalRows = options != nullptr ? static_cast<int>(options->size()) : 0;
        int preferredPickerRows = kPickerMaxVisibleRows;
        switch (this->openPicker)
        {
            case AppearancePickerType::REPAIR_STYLE:
            case AppearancePickerType::SPEED_STYLE:
            case AppearancePickerType::EXPLOSION_STYLE:
            case AppearancePickerType::ROCKET_STYLE:
            case AppearancePickerType::PROJECTILE_STYLE:
            case AppearancePickerType::MOVE_CLICK_STYLE:
                preferredPickerRows = kEffectPickerMaxVisibleRows;
                break;
            default:
                break;
        }

        const bool openUpward = this->openPicker == AppearancePickerType::EMOTE;
        const PickerLayout pickerLayout = buildPickerLayout(
            this->widgetRect,
            this->openPickerAnchorRect,
            totalRows,
            preferredPickerRows,
            openUpward);
        this->pickerFirstRow = (std::max)(0, (std::min)(this->pickerFirstRow, pickerLayout.maxFirstRow));

        if (isPointInRect(x, y, pickerLayout.panel))
        {
            if (pickerLayout.maxFirstRow > 0 && isPointInRect(x, y, pickerLayout.scrollTrack))
            {
                const float thumbHeight = (std::max)(
                    kMinThumbHeight,
                    (pickerLayout.scrollTrack.h * static_cast<float>(pickerLayout.visibleRows) / static_cast<float>((std::max)(1, pickerLayout.totalRows))));
                const float thumbTravel = (std::max)(1.0f, pickerLayout.scrollTrack.h - thumbHeight);
                const float ratio = static_cast<float>(this->pickerFirstRow) / static_cast<float>((std::max)(1, pickerLayout.maxFirstRow));
                const float thumbY = pickerLayout.scrollTrack.y + (thumbTravel * ratio);
                const SDL_FRect thumb = SDL_FRect{pickerLayout.scrollTrack.x, thumbY, pickerLayout.scrollTrack.w, thumbHeight};

                this->pickerScrollDragging = true;
                this->fleetScrollDragging = false;
                this->widgetDragging = false;

                if (isPointInRect(x, y, thumb))
                {
                    this->pickerScrollDragOffsetY = y - thumb.y;
                }
                else
                {
                    this->pickerScrollDragOffsetY = thumbHeight * 0.5f;
                    const float thumbTop = clampf(y - this->pickerScrollDragOffsetY, pickerLayout.scrollTrack.y, pickerLayout.scrollTrack.y + thumbTravel);
                    const float t = (thumbTop - pickerLayout.scrollTrack.y) / thumbTravel;
                    this->pickerFirstRow = static_cast<int>(t * static_cast<float>(pickerLayout.maxFirstRow) + 0.5f);
                    this->pickerFirstRow = (std::max)(0, (std::min)(this->pickerFirstRow, pickerLayout.maxFirstRow));
                }
                return true;
            }

            const int firstRow = this->pickerFirstRow;
            const int lastRow = (std::min)(pickerLayout.totalRows, firstRow + pickerLayout.visibleRows);
            for (int row = firstRow; row < lastRow; ++row)
            {
                const int visualRow = row - firstRow;
                const SDL_FRect rowRect = SDL_FRect{
                    pickerLayout.body.x,
                    pickerLayout.body.y + (static_cast<float>(visualRow) * kPickerRowHeight),
                    getPickerListRowWidth(pickerLayout),
                    kPickerRowHeight
                };
                if (!isPointInRect(x, y, rowRect))
                {
                    continue;
                }

                if (selectedIndex != nullptr)
                {
                    *selectedIndex = row;
                }

                this->closeAppearancePicker();
                return true;
            }

            return true;
        }

        this->closeAppearancePicker();
    }

    if (this->activeTab == ActiveTab::ACCOUNT)
    {
        if (isPointInRect(x, y, layout.accountProfileNameInput))
        {
            this->profileInputFocused = true;
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;

            const float localX = (std::max)(0.0f, x - (layout.accountProfileNameInput.x + 8.0f));
            std::size_t cursor = 0;
            for (std::size_t i = 0; i <= this->profileName.size(); ++i)
            {
                const float w = measureTextWidth(&this->bodyFont, this->profileName.substr(0, i));
                if (localX <= w)
                {
                    cursor = i;
                    break;
                }
                cursor = i;
            }
            this->profileCursorIndex = (std::min)(cursor, this->profileName.size());
            return true;
        }

        if (isPointInRect(x, y, layout.accountProfileApplyButton))
        {
            this->clearProfileInputFocus();
            return true;
        }

        this->clearProfileInputFocus();
        return true;
    }

    if (this->activeTab == ActiveTab::APPEARANCE)
    {
        if (isPointInRect(x, y, layout.appearanceCoatingCase))
        {
            if (this->openPicker == AppearancePickerType::SHIP_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::SHIP_STYLE, layout.appearanceCoatingCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceRepairCase))
        {
            if (this->openPicker == AppearancePickerType::REPAIR_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::REPAIR_STYLE, layout.appearanceRepairCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceSpeedCase))
        {
            if (this->openPicker == AppearancePickerType::SPEED_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::SPEED_STYLE, layout.appearanceSpeedCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceExplosionCase))
        {
            if (this->openPicker == AppearancePickerType::EXPLOSION_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::EXPLOSION_STYLE, layout.appearanceExplosionCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceRocketCase))
        {
            if (this->openPicker == AppearancePickerType::ROCKET_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::ROCKET_STYLE, layout.appearanceRocketCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceProjectileCase))
        {
            if (this->openPicker == AppearancePickerType::PROJECTILE_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::PROJECTILE_STYLE, layout.appearanceProjectileCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceMoveClickCase))
        {
            if (this->openPicker == AppearancePickerType::MOVE_CLICK_STYLE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::MOVE_CLICK_STYLE, layout.appearanceMoveClickCase);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.appearanceEmotesCase))
        {
            if (this->openPicker == AppearancePickerType::EMOTE)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::EMOTE, layout.appearanceEmotesCase);
            }
            return true;
        }

        this->closeAppearancePicker();
        return true;
    }

    if (this->activeTab == ActiveTab::SHIP_MANAGEMENT)
    {
        if (isPointInRect(x, y, layout.shipManagementShipCase))
        {
            if (this->openPicker == AppearancePickerType::SHIP_BONUS)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::SHIP_BONUS, layout.shipManagementShipCase);
            }
            return true;
        }

        this->closeAppearancePicker();
        return true;
    }

    if (this->activeTab == ActiveTab::STORAGE_EQUIPPED)
    {
        if (isPointInRect(x, y, layout.storageLeftDropdown))
        {
            if (this->openPicker == AppearancePickerType::STORAGE_ITEM_LEFT)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::STORAGE_ITEM_LEFT, layout.storageLeftDropdown);
            }
            return true;
        }

        if (isPointInRect(x, y, layout.storageMiddleDropdown))
        {
            if (this->openPicker == AppearancePickerType::STORAGE_ITEM_RIGHT)
            {
                this->closeAppearancePicker();
            }
            else
            {
                this->openAppearancePickerForRect(AppearancePickerType::STORAGE_ITEM_RIGHT, layout.storageMiddleDropdown);
            }
            return true;
        }

        this->closeAppearancePicker();
        return true;
    }

    if (this->activeTab != ActiveTab::ELITE_SHIPS && this->activeTab != ActiveTab::SPECIAL_SHIPS)
    {
        return true;
    }

    std::vector<ShipEntry>* ships = this->getShipsForTab(this->activeTab);
    int* firstRowPtr = this->getFirstRowForTab(this->activeTab);
    if (ships == nullptr || firstRowPtr == nullptr)
    {
        return true;
    }

    const SDL_FRect viewport = (this->activeTab == ActiveTab::ELITE_SHIPS)
        ? layout.fleetGridViewportElite
        : layout.fleetGridViewportSpecial;
    const FleetMetrics metrics = buildFleetMetrics(viewport, this->getVisibleShipCountForTab(this->activeTab));

    int& firstRow = *firstRowPtr;
    firstRow = (std::max)(0, (std::min)(firstRow, metrics.maxFirstRow));

    if (isPointInRect(x, y, viewport) && metrics.maxFirstRow > 0 && isPointInRect(x, y, metrics.scrollTrack))
    {
        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (metrics.scrollTrack.h * static_cast<float>(metrics.visibleRows) / static_cast<float>((std::max)(1, metrics.totalRows))));
        const float thumbTravel = (std::max)(1.0f, metrics.scrollTrack.h - thumbHeight);
        const float ratio = static_cast<float>(firstRow) / static_cast<float>((std::max)(1, metrics.maxFirstRow));
        const float thumbY = metrics.scrollTrack.y + (thumbTravel * ratio);
        const SDL_FRect thumb = SDL_FRect{metrics.scrollTrack.x, thumbY, metrics.scrollTrack.w, thumbHeight};

        this->fleetScrollDragging = true;
        this->pickerScrollDragging = false;
        this->widgetDragging = false;

        if (isPointInRect(x, y, thumb))
        {
            this->fleetScrollDragOffsetY = y - thumb.y;
        }
        else
        {
            this->fleetScrollDragOffsetY = thumbHeight * 0.5f;
            const float thumbTop = clampf(y - this->fleetScrollDragOffsetY, metrics.scrollTrack.y, metrics.scrollTrack.y + thumbTravel);
            const float t = (thumbTop - metrics.scrollTrack.y) / thumbTravel;
            firstRow = static_cast<int>(t * static_cast<float>(metrics.maxFirstRow) + 0.5f);
            firstRow = (std::max)(0, (std::min)(firstRow, metrics.maxFirstRow));
        }
        return true;
    }

    // Onglets navires acquis en lecture seule: pas de selection sur clic carte.
    return true;
}

bool AccountManagementWidget::mousewheelmoved(
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

    if (!isPointInRect(mouse_x, mouse_y, this->widgetRect))
    {
        return false;
    }

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

    if (delta == 0)
    {
        return true;
    }

    if (this->openPicker != AppearancePickerType::NONE)
    {
        const std::vector<AppearanceOption>* options = this->getOptionsForPicker(this->openPicker);
        const int totalRows = options != nullptr ? static_cast<int>(options->size()) : 0;
        int preferredPickerRows = kPickerMaxVisibleRows;
        switch (this->openPicker)
        {
            case AppearancePickerType::REPAIR_STYLE:
            case AppearancePickerType::SPEED_STYLE:
            case AppearancePickerType::EXPLOSION_STYLE:
            case AppearancePickerType::ROCKET_STYLE:
            case AppearancePickerType::PROJECTILE_STYLE:
            case AppearancePickerType::MOVE_CLICK_STYLE:
                preferredPickerRows = kEffectPickerMaxVisibleRows;
                break;
            default:
                break;
        }

        const bool openUpward = this->openPicker == AppearancePickerType::EMOTE;
        const PickerLayout pickerLayout = buildPickerLayout(
            this->widgetRect,
            this->openPickerAnchorRect,
            totalRows,
            preferredPickerRows,
            openUpward);
        if (isPointInRect(mouse_x, mouse_y, pickerLayout.panel))
        {
            if (pickerLayout.maxFirstRow > 0)
            {
                const int rowBefore = this->pickerFirstRow;
                this->pickerFirstRow -= delta;
                this->pickerFirstRow = (std::max)(0, (std::min)(this->pickerFirstRow, pickerLayout.maxFirstRow));
                if (this->pickerFirstRow != rowBefore)
                {
                    this->pickerScrollWheelHighlightSec = kScrollThumbWheelHighlightSec;
                }
            }
            return true;
        }
    }

    if (this->activeTab == ActiveTab::ELITE_SHIPS || this->activeTab == ActiveTab::SPECIAL_SHIPS)
    {
        std::vector<ShipEntry>* ships = this->getShipsForTab(this->activeTab);
        int* firstRowPtr = this->getFirstRowForTab(this->activeTab);
        if (ships == nullptr || firstRowPtr == nullptr)
        {
            return true;
        }

        const WidgetLayout layout = buildLayout(this->widgetRect);
        const SDL_FRect viewport = (this->activeTab == ActiveTab::ELITE_SHIPS)
            ? layout.fleetGridViewportElite
            : layout.fleetGridViewportSpecial;
        if (!isPointInRect(mouse_x, mouse_y, viewport))
        {
            return true;
        }

        const FleetMetrics metrics = buildFleetMetrics(viewport, this->getVisibleShipCountForTab(this->activeTab));
        int& firstRow = *firstRowPtr;
        if (metrics.maxFirstRow > 0)
        {
            const int rowBefore = firstRow;
            firstRow -= delta;
            firstRow = (std::max)(0, (std::min)(firstRow, metrics.maxFirstRow));
            if (firstRow != rowBefore)
            {
                this->fleetScrollWheelHighlightSec = kScrollThumbWheelHighlightSec;
            }
        }
        return true;
    }

    return true;
}

bool AccountManagementWidget::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)isrepeat;

    if (!this->visible || !this->profileInputFocused)
    {
        return false;
    }

    switch (scancode)
    {
        case SDL_SCANCODE_LEFT:
            if (this->profileCursorIndex > 0) { --this->profileCursorIndex; }
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RIGHT:
            if (this->profileCursorIndex < this->profileName.size()) { ++this->profileCursorIndex; }
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_HOME:
            this->profileCursorIndex = 0;
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_END:
            this->profileCursorIndex = this->profileName.size();
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_BACKSPACE:
            if (this->profileCursorIndex > 0 && !this->profileName.empty())
            {
                this->profileName.erase(this->profileCursorIndex - 1, 1);
                --this->profileCursorIndex;
            }
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_DELETE:
            if (this->profileCursorIndex < this->profileName.size())
            {
                this->profileName.erase(this->profileCursorIndex, 1);
            }
            this->profileCursorVisible = true;
            this->profileCursorBlinkElapsed = 0.0;
            return true;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            this->clearProfileInputFocus();
            return true;
        case SDL_SCANCODE_ESCAPE:
            this->clearProfileInputFocus();
            return true;
        default:
            break;
    }

    char newCharacter = '\0';
    if (keyToPrintableChar(key, scancode, keycode, mod, &newCharacter))
    {
        if (this->profileName.size() >= static_cast<std::size_t>(kMaxProfileNameChars))
        {
            return true;
        }

        this->profileCursorIndex = (std::min)(this->profileCursorIndex, this->profileName.size());
        this->profileName.insert(this->profileCursorIndex, 1, newCharacter);
        ++this->profileCursorIndex;
        this->profileCursorVisible = true;
        this->profileCursorBlinkElapsed = 0.0;
        return true;
    }

    return false;
}

void AccountManagementWidget::draw(void) const
{
    AccountManagementWidget* self = const_cast<AccountManagementWidget*>(this);

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

    const WidgetLayout layout = buildLayout(self->widgetRect);

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

    auto drawTab = [&](const SDL_FRect& r, const char* label, bool active) {
        rc2d_graphics_setColor(active ? kTabActive : kTabInactive);
        rc2d_graphics_rectangle("fill", &r);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &r);
        drawCentered(&self->smallFont, label, r, active ? kTextGold : kTextMuted);
    };

    drawTab(layout.tabAccount, "Compte", self->activeTab == ActiveTab::ACCOUNT);
    drawTab(layout.tabAppearance, "Apparence", self->activeTab == ActiveTab::APPEARANCE);
    drawTab(layout.tabShipManagement, "Gestion du navire", self->activeTab == ActiveTab::SHIP_MANAGEMENT);
    drawTab(layout.tabElite, "Navires elite acquis", self->activeTab == ActiveTab::ELITE_SHIPS);
    drawTab(layout.tabSpecial, "Navires speciaux acquis", self->activeTab == ActiveTab::SPECIAL_SHIPS);
    drawTab(layout.tabStorageEquipped, "Entrepot / Equipe", self->activeTab == ActiveTab::STORAGE_EQUIPPED);

    self->controlIcons.drawCloseButton(layout.closeButton, kHeaderFill, kGold);

    if (self->activeTab == ActiveTab::ACCOUNT)
    {
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.accountLeftInfo);
        rc2d_graphics_rectangle("fill", &layout.accountLeftStats);
        rc2d_graphics_rectangle("fill", &layout.accountLeftPremium);
        rc2d_graphics_rectangle("fill", &layout.accountRightProfile);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.accountLeftInfo);
        rc2d_graphics_rectangle("line", &layout.accountLeftStats);
        rc2d_graphics_rectangle("line", &layout.accountLeftPremium);
        rc2d_graphics_rectangle("line", &layout.accountRightProfile);

        drawCentered(
            &self->smallFont,
            "Info",
            SDL_FRect{layout.accountLeftInfo.x, layout.accountLeftInfo.y - 18.0f, layout.accountLeftInfo.w, 18.0f},
            kTextGold);
        drawCentered(
            &self->smallFont,
            "Modifier le profil",
            SDL_FRect{layout.accountRightProfile.x, layout.accountRightProfile.y - 18.0f, layout.accountRightProfile.w, 18.0f},
            kTextGold);

        auto drawAccountRows = [&](const SDL_FRect& panel, const std::vector<std::pair<std::string, std::string>>& rows) {
            if (rows.empty())
            {
                return;
            }

            const float sidePad = 10.0f;
            const float topPad = 8.0f;
            const float bottomPad = 8.0f;
            const float textHalfHeight = 9.0f;
            const float lineLeft = panel.x + 8.0f;
            const float lineRight = panel.x + panel.w - 8.0f;
            const float contentH = (std::max)(1.0f, panel.h - topPad - bottomPad);
            const float rowH = contentH / static_cast<float>(rows.size());

            for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            {
                const float rowTop = panel.y + topPad + (rowH * static_cast<float>(i));
                const float rowCenterY = rowTop + (rowH * 0.5f);
                const float textY = std::round(rowCenterY - textHalfHeight);

                drawTextAt(&self->bodyFont, rows[static_cast<std::size_t>(i)].first, panel.x + sidePad, textY, kTextGold);

                const std::string& value = rows[static_cast<std::size_t>(i)].second;
                const float valueW = measureTextWidth(&self->bodyFont, value);
                drawTextAt(&self->bodyFont, value, panel.x + panel.w - sidePad - valueW, textY, kTextBody);

                if (i < static_cast<int>(rows.size()) - 1)
                {
                    const float lineY = panel.y + topPad + (rowH * static_cast<float>(i + 1));
                    rc2d_graphics_setColor(kRowLine);
                    rc2d_graphics_line(lineLeft, lineY, lineRight, lineY);
                }
            }
        };

        const std::string playerIdentifierText = self->playerIdentifier.empty()
            ? std::string("-")
            : self->playerIdentifier;
        const std::string pirateSinceValue = self->pirateSinceText.empty()
            ? std::string("-")
            : self->pirateSinceText;
        const std::string premiumSinceValue = self->premiumSinceText.empty()
            ? std::string("-")
            : self->premiumSinceText;

        drawAccountRows(
            layout.accountLeftInfo,
            {
                {"IDJoueur", playerIdentifierText},
                {"Pirate since", pirateSinceValue}
            });

        drawAccountRows(
            layout.accountLeftStats,
            {
                {"Niveau", formatWithDots(self->playerLevel)},
                {"Points d'experience", formatWithDots(self->experiencePointsCurrent)},
                {"Points d'elite", formatWithDots(self->eliteProgressData.currentPoints)},
                {"Points de combat", formatWithDots(self->combatPointsCurrent)}
            });

        drawAccountRows(
            layout.accountLeftPremium,
            {
                {"Premium depuis", premiumSinceValue}
            });

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &layout.accountProfileNameInput);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.accountProfileNameInput);
        drawTextAt(&self->bodyFont, self->profileName, layout.accountProfileNameInput.x + 8.0f, layout.accountProfileNameInput.y + 6.0f, kTextGold);

        if (self->profileInputFocused && self->profileCursorVisible)
        {
            const std::size_t cursor = (std::min)(self->profileCursorIndex, self->profileName.size());
            const std::string prefix = self->profileName.substr(0, cursor);
            const float cx = layout.accountProfileNameInput.x + 8.0f + measureTextWidth(&self->bodyFont, prefix);
            rc2d_graphics_setColor(kTextGold);
            rc2d_graphics_line(cx, layout.accountProfileNameInput.y + 6.0f, cx, layout.accountProfileNameInput.y + layout.accountProfileNameInput.h - 6.0f);
        }

        rc2d_graphics_setColor(kButtonFill);
        rc2d_graphics_rectangle("fill", &layout.accountProfileApplyButton);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.accountProfileApplyButton);
        drawCentered(&self->smallFont, "Changer de nom", layout.accountProfileApplyButton, kTextBody);
    }
    else if (self->activeTab == ActiveTab::APPEARANCE)
    {
        auto drawSectionHeader = [&](const SDL_FRect& r, const char* label) {
            rc2d_graphics_setColor(kHeaderFill);
            rc2d_graphics_rectangle("fill", &r);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &r);
            drawTextAt(&self->bodyFont, label, r.x + 10.0f, r.y + 6.0f, kTextGold);
        };

        auto drawAppearanceCase = [&](const SDL_FRect& r,
                                      const char* title,
                                      const std::string& subtitle,
                                      RC2D_Image* icon,
                                      bool active) {
            rc2d_graphics_setColor(active ? kSelectionFill : kPanelFill);
            rc2d_graphics_rectangle("fill", &r);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &r);

            const SDL_FRect iconRect = SDL_FRect{r.x + 12.0f, r.y + ((r.h - 78.0f) * 0.5f), 78.0f, 78.0f};
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &iconRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &iconRect);
            drawImageFit(icon, iconRect);

            drawTextAt(&self->bodyFont, title, iconRect.x + iconRect.w + 12.0f, r.y + 20.0f, kTextGold);
            drawTextAt(&self->smallFont, subtitle, iconRect.x + iconRect.w + 12.0f, r.y + 50.0f, kTextBody);
        };

        drawSectionHeader(layout.appearanceCoatingHeader, "Apparence actuelle du navire");
        drawSectionHeader(layout.appearanceEffectsHeader, "Apparence actuelle des effets visuels");
        drawSectionHeader(layout.appearanceEmotesHeader, "Emotes");

        int coatingIndex = (std::max)(0, (std::min)(self->selectedCoatingOption, static_cast<int>(self->coatingOptions.size()) - 1));
        int repairIndex = (std::max)(0, (std::min)(self->selectedRepairStyleOption, static_cast<int>(self->repairStyleOptions.size()) - 1));
        int speedIndex = (std::max)(0, (std::min)(self->selectedSpeedStyleOption, static_cast<int>(self->speedStyleOptions.size()) - 1));
        int explosionIndex = (std::max)(0, (std::min)(self->selectedExplosionStyleOption, static_cast<int>(self->explosionStyleOptions.size()) - 1));
        int rocketIndex = (std::max)(0, (std::min)(self->selectedRocketStyleOption, static_cast<int>(self->rocketStyleOptions.size()) - 1));
        int projectileIndex = (std::max)(0, (std::min)(self->selectedProjectileStyleOption, static_cast<int>(self->projectileStyleOptions.size()) - 1));
        int moveClickIndex = (std::max)(0, (std::min)(self->selectedMoveClickStyleOption, static_cast<int>(self->moveClickStyleOptions.size()) - 1));

        RC2D_Image* coatingIcon = (coatingIndex >= 0 && coatingIndex < static_cast<int>(self->coatingOptionIcons.size())) ? &self->coatingOptionIcons[static_cast<std::size_t>(coatingIndex)] : nullptr;
        RC2D_Image* repairIcon = (repairIndex >= 0 && repairIndex < static_cast<int>(self->repairStyleOptionIcons.size())) ? &self->repairStyleOptionIcons[static_cast<std::size_t>(repairIndex)] : nullptr;
        RC2D_Image* speedIcon = (speedIndex >= 0 && speedIndex < static_cast<int>(self->speedStyleOptionIcons.size())) ? &self->speedStyleOptionIcons[static_cast<std::size_t>(speedIndex)] : nullptr;
        RC2D_Image* explosionIcon = (explosionIndex >= 0 && explosionIndex < static_cast<int>(self->explosionStyleOptionIcons.size())) ? &self->explosionStyleOptionIcons[static_cast<std::size_t>(explosionIndex)] : nullptr;
        RC2D_Image* rocketIcon = (rocketIndex >= 0 && rocketIndex < static_cast<int>(self->rocketStyleOptionIcons.size())) ? &self->rocketStyleOptionIcons[static_cast<std::size_t>(rocketIndex)] : nullptr;
        RC2D_Image* projectileIcon = (projectileIndex >= 0 && projectileIndex < static_cast<int>(self->projectileStyleOptionIcons.size())) ? &self->projectileStyleOptionIcons[static_cast<std::size_t>(projectileIndex)] : nullptr;
        RC2D_Image* moveClickIcon = (moveClickIndex >= 0 && moveClickIndex < static_cast<int>(self->moveClickStyleOptionIcons.size())) ? &self->moveClickStyleOptionIcons[static_cast<std::size_t>(moveClickIndex)] : nullptr;

        const std::string coatingSubtitle = (coatingIndex >= 0 && coatingIndex < static_cast<int>(self->coatingOptions.size())) ? self->coatingOptions[static_cast<std::size_t>(coatingIndex)].name : std::string("-");
        const std::string repairSubtitle = (repairIndex >= 0 && repairIndex < static_cast<int>(self->repairStyleOptions.size())) ? self->repairStyleOptions[static_cast<std::size_t>(repairIndex)].name : std::string("-");
        const std::string speedSubtitle = (speedIndex >= 0 && speedIndex < static_cast<int>(self->speedStyleOptions.size())) ? self->speedStyleOptions[static_cast<std::size_t>(speedIndex)].name : std::string("-");
        const std::string explosionSubtitle = (explosionIndex >= 0 && explosionIndex < static_cast<int>(self->explosionStyleOptions.size())) ? self->explosionStyleOptions[static_cast<std::size_t>(explosionIndex)].name : std::string("-");
        const std::string rocketSubtitle = (rocketIndex >= 0 && rocketIndex < static_cast<int>(self->rocketStyleOptions.size())) ? self->rocketStyleOptions[static_cast<std::size_t>(rocketIndex)].name : std::string("-");
        const std::string projectileSubtitle = (projectileIndex >= 0 && projectileIndex < static_cast<int>(self->projectileStyleOptions.size())) ? self->projectileStyleOptions[static_cast<std::size_t>(projectileIndex)].name : std::string("-");
        const std::string moveClickSubtitle = (moveClickIndex >= 0 && moveClickIndex < static_cast<int>(self->moveClickStyleOptions.size())) ? self->moveClickStyleOptions[static_cast<std::size_t>(moveClickIndex)].name : std::string("-");

        drawAppearanceCase(layout.appearanceCoatingCase, "Style du navire", coatingSubtitle, coatingIcon, self->openPicker == AppearancePickerType::SHIP_STYLE);
        drawAppearanceCase(layout.appearanceRepairCase, "Style de reparation", repairSubtitle, repairIcon, self->openPicker == AppearancePickerType::REPAIR_STYLE);
        drawAppearanceCase(layout.appearanceSpeedCase, "Style de vitesse", speedSubtitle, speedIcon, self->openPicker == AppearancePickerType::SPEED_STYLE);
        drawAppearanceCase(layout.appearanceExplosionCase, "Style d'impact des boulets", explosionSubtitle, explosionIcon, self->openPicker == AppearancePickerType::EXPLOSION_STYLE);
        drawAppearanceCase(layout.appearanceRocketCase, "Style des fusees", rocketSubtitle, rocketIcon, self->openPicker == AppearancePickerType::ROCKET_STYLE);
        drawAppearanceCase(layout.appearanceProjectileCase, "Style des projectiles", projectileSubtitle, projectileIcon, self->openPicker == AppearancePickerType::PROJECTILE_STYLE);
        drawAppearanceCase(layout.appearanceMoveClickCase, "Style des clics de deplacement", moveClickSubtitle, moveClickIcon, self->openPicker == AppearancePickerType::MOVE_CLICK_STYLE);

        rc2d_graphics_setColor(self->openPicker == AppearancePickerType::EMOTE ? kSelectionFill : kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.appearanceEmotesCase);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.appearanceEmotesCase);

        for (int i = 0; i < static_cast<int>(layout.appearanceEmoteSlots.size()); ++i)
        {
            const SDL_FRect slot = layout.appearanceEmoteSlots[static_cast<std::size_t>(i)];
            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &slot);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &slot);

            if (!self->emoteOptionIcons.empty())
            {
                const int iconIndex = (self->selectedEmoteOption + i) % static_cast<int>(self->emoteOptionIcons.size());
                drawImageFit(&self->emoteOptionIcons[static_cast<std::size_t>(iconIndex)], slot);
            }
        }
    }
    else if (self->activeTab == ActiveTab::SHIP_MANAGEMENT)
    {
        rc2d_graphics_setColor(kHeaderFill);
        rc2d_graphics_rectangle("fill", &layout.shipManagementHeader);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.shipManagementHeader);
        drawTextAt(&self->bodyFont, "Gestion du navire", layout.shipManagementHeader.x + 10.0f, layout.shipManagementHeader.y + 6.0f, kTextGold);

        const int shipIndex = (std::max)(0, (std::min)(self->selectedShipOption, static_cast<int>(self->shipOptions.size()) - 1));
        RC2D_Image* shipIcon = (shipIndex >= 0 && shipIndex < static_cast<int>(self->shipOptionIcons.size()))
            ? &self->shipOptionIcons[static_cast<std::size_t>(shipIndex)]
            : nullptr;
        const std::string shipSubtitle = (shipIndex >= 0 && shipIndex < static_cast<int>(self->shipOptions.size()))
            ? self->shipOptions[static_cast<std::size_t>(shipIndex)].name
            : std::string("-");

        rc2d_graphics_setColor(self->openPicker == AppearancePickerType::SHIP_BONUS ? kSelectionFill : kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.shipManagementShipCase);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.shipManagementShipCase);

        const SDL_FRect iconRect = SDL_FRect{
            layout.shipManagementShipCase.x + 12.0f,
            layout.shipManagementShipCase.y + ((layout.shipManagementShipCase.h - 78.0f) * 0.5f),
            78.0f,
            78.0f
        };
        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &iconRect);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &iconRect);
        drawImageFit(shipIcon, iconRect);

        drawTextAt(&self->bodyFont, "Bonus du navire", iconRect.x + iconRect.w + 12.0f, layout.shipManagementShipCase.y + 20.0f, kTextGold);
        drawTextAt(&self->smallFont, shipSubtitle, iconRect.x + iconRect.w + 12.0f, layout.shipManagementShipCase.y + 50.0f, kTextBody);
    }
    else if (self->activeTab == ActiveTab::STORAGE_EQUIPPED)
    {
        auto drawStoragePanel = [&](const SDL_FRect& r) {
            rc2d_graphics_setColor(kPanelFill);
            rc2d_graphics_rectangle("fill", &r);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &r);
        };
        auto drawStorageDropdown = [&](const SDL_FRect& r, const std::string& label, bool active) {
            rc2d_graphics_setColor(active ? kSelectionFill : kHeaderFill);
            rc2d_graphics_rectangle("fill", &r);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &r);

            drawTextAt(&self->bodyFont, label, r.x + 10.0f, r.y + 8.0f, kTextGold);
            drawTextAt(&self->bodyFont, "v", r.x + r.w - 16.0f, r.y + 6.0f, kTextGold);
        };

        drawStoragePanel(layout.storageLeftPanel);
        drawStoragePanel(layout.storageMiddlePanel);
        drawStoragePanel(layout.storageRightPanel);

        drawCentered(
            &self->smallFont,
            "Entrepot",
            SDL_FRect{layout.storageLeftPanel.x, layout.storageLeftPanel.y - 18.0f, layout.storageLeftPanel.w, 18.0f},
            kTextGold);
        drawCentered(
            &self->smallFont,
            "Equipe",
            SDL_FRect{layout.storageMiddlePanel.x, layout.storageMiddlePanel.y - 18.0f, layout.storageMiddlePanel.w, 18.0f},
            kTextGold);
        drawCentered(
            &self->smallFont,
            "Valeurs equipees",
            SDL_FRect{layout.storageRightPanel.x, layout.storageRightPanel.y - 18.0f, layout.storageRightPanel.w, 18.0f},
            kTextGold);

        const int equipmentIndex = self->storageEquipmentOptions.empty()
            ? -1
            : (std::max)(0, (std::min)(self->selectedStorageEquipmentOption, static_cast<int>(self->storageEquipmentOptions.size()) - 1));
        const std::string equipmentLabel = (equipmentIndex >= 0 && equipmentIndex < static_cast<int>(self->storageEquipmentOptions.size()))
            ? self->storageEquipmentOptions[static_cast<std::size_t>(equipmentIndex)].name
            : std::string("-");
        RC2D_Image* equipmentIcon = (equipmentIndex >= 0 && equipmentIndex < static_cast<int>(self->storageEquipmentOptionIcons.size()))
            ? &self->storageEquipmentOptionIcons[static_cast<std::size_t>(equipmentIndex)]
            : nullptr;

        drawStorageDropdown(
            layout.storageLeftDropdown,
            equipmentLabel,
            self->openPicker == AppearancePickerType::STORAGE_ITEM_LEFT);
        drawStorageDropdown(
            layout.storageMiddleDropdown,
            equipmentLabel,
            self->openPicker == AppearancePickerType::STORAGE_ITEM_RIGHT);

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &layout.storageLeftContent);
        rc2d_graphics_rectangle("fill", &layout.storageMiddleContent);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &layout.storageLeftContent);
        rc2d_graphics_rectangle("line", &layout.storageMiddleContent);

        const SDL_FRect storageIconLeft = SDL_FRect{
            layout.storageLeftContent.x + 10.0f,
            layout.storageLeftContent.y + 10.0f,
            64.0f,
            64.0f
        };
        const SDL_FRect storageIconRight = SDL_FRect{
            layout.storageMiddleContent.x + layout.storageMiddleContent.w - 74.0f,
            layout.storageMiddleContent.y + 10.0f,
            64.0f,
            64.0f
        };
        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &storageIconLeft);
        rc2d_graphics_rectangle("fill", &storageIconRight);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &storageIconLeft);
        rc2d_graphics_rectangle("line", &storageIconRight);
        drawImageFit(equipmentIcon, storageIconLeft);
        drawImageFit(equipmentIcon, storageIconRight);

        std::vector<std::pair<std::string, std::string>> valueRows;
        if (equipmentIndex < 0)
        {
            valueRows = {
                {"Aucune categorie", "Aucune donnee alimentee"}
            };
        }
        else if (equipmentIndex == 1)
        {
            valueRows = {
                {"Harponneuse", "Aucun n'est equipe"},
                {"Champ de tir de harpons", "+4,0"},
                {"Temps de recharge du harpon", "+5,0/s"},
                {"Degats critiques du harpon", "+0%"},
                {"Probabilite critique du harpon", "+5,0%"},
                {"Sante", "+0"},
                {"Montant de la reparation", "+0"}
            };
        }
        else if (equipmentIndex == 2)
        {
            valueRows = {
                {"Voiles", "Aucune n'est equipee"},
                {"Points de voiles", "+0"},
                {"Acceleration", "+0%"},
                {"Vitesse max", "+0%"},
                {"Maniabilite", "+0"},
                {"Durabilite", "+0"}
            };
        }
        else
        {
            valueRows = {
                {"Cannons", "Aucun n'est equipe"},
                {"Canons correcteurs", "Aucun n'est equipe"},
                {"Degats des cannons", "+0%"},
                {"Portee des cannons", "+0"},
                {"Precision des cannons", "+0%"},
                {"Temps de recharge des cannons", "+0,0/s"}
            };
        }

        const float rightInnerPad = 1.0f;
        const SDL_FRect valueTopBand = SDL_FRect{
            layout.storageRightPanel.x + rightInnerPad,
            layout.storageRightPanel.y + rightInnerPad,
            layout.storageRightPanel.w - (rightInnerPad * 2.0f),
            34.0f
        };
        rc2d_graphics_setColor(kHeaderFill);
        rc2d_graphics_rectangle("fill", &valueTopBand);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &valueTopBand);
        drawTextAt(&self->bodyFont, equipmentLabel, valueTopBand.x + 8.0f, valueTopBand.y + 8.0f, kTextGold);

        const float valuesStartY = valueTopBand.y + valueTopBand.h + 2.0f;
        const float valuesAvailableH = (layout.storageRightPanel.y + layout.storageRightPanel.h - 1.0f) - valuesStartY;
        const float rowH = valueRows.empty() ? 0.0f : (valuesAvailableH / static_cast<float>(valueRows.size()));
        for (int i = 0; i < static_cast<int>(valueRows.size()); ++i)
        {
            SDL_FRect rowRect = SDL_FRect{
                layout.storageRightPanel.x + rightInnerPad,
                valuesStartY + (rowH * static_cast<float>(i)),
                layout.storageRightPanel.w - (rightInnerPad * 2.0f),
                rowH
            };

            rc2d_graphics_setColor(kFieldFill);
            rc2d_graphics_rectangle("fill", &rowRect);
            rc2d_graphics_setColor(kRowLine);
            rc2d_graphics_rectangle("line", &rowRect);

            const std::string& title = valueRows[static_cast<std::size_t>(i)].first;
            const std::string& value = valueRows[static_cast<std::size_t>(i)].second;
            drawTextAt(&self->bodyFont, title, rowRect.x + 8.0f, rowRect.y + 8.0f, kTextGold);
            const RC2D_Color valueColor = (!value.empty() && value[0] == '+')
                ? RC2D_Color{82, 204, 106, 255}
                : kTextBody;
            drawTextAt(&self->bodyFont, value, rowRect.x + 8.0f, rowRect.y + 30.0f, valueColor);
        }
    }
    else
    {
        const bool eliteTab = self->activeTab == ActiveTab::ELITE_SHIPS;
        std::vector<ShipEntry>* ships = self->getShipsForTab(self->activeTab);
        std::vector<RC2D_Image>* shipIcons = self->getShipIconsForTab(self->activeTab);
        int* firstRowPtr = self->getFirstRowForTab(self->activeTab);

        if (ships != nullptr && shipIcons != nullptr && firstRowPtr != nullptr)
        {
            const SDL_FRect viewport = eliteTab ? layout.fleetGridViewportElite : layout.fleetGridViewportSpecial;
            const int visibleShipCount = self->getVisibleShipCountForTab(self->activeTab);
            const FleetMetrics metrics = buildFleetMetrics(viewport, visibleShipCount);
            int& firstRow = *firstRowPtr;
            firstRow = (std::max)(0, (std::min)(firstRow, metrics.maxFirstRow));

            rc2d_graphics_setColor(kHeaderFill);
            rc2d_graphics_rectangle("fill", &layout.fleetHeader);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &layout.fleetHeader);
            drawTextAt(
                &self->bodyFont,
                eliteTab ? "Navire elite acquis" : "Navire speciaux acquis",
                layout.fleetHeader.x + 10.0f,
                layout.fleetHeader.y + 6.0f,
                kTextGold);

            if (eliteTab)
            {
                rc2d_graphics_setColor(kHeaderFill);
                rc2d_graphics_rectangle("fill", &layout.fleetPointsPanel);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("line", &layout.fleetPointsPanel);

                const EliteProgressData eliteProgress = self->getEliteProgress();
                const int currentElitePoints = eliteProgress.currentPoints;
                const int nextEliteTargetPoints = eliteProgress.hasNextShip
                    ? eliteProgress.nextShipPointsRequired
                    : (std::max)(currentElitePoints, 1);
                const float ratio = eliteProgress.hasNextShip
                    ? clampf(
                          static_cast<float>(currentElitePoints) /
                              static_cast<float>((std::max)(1, nextEliteTargetPoints)),
                          0.0f,
                          1.0f)
                    : 1.0f;
                const bool hasEliteData = !self->eliteShips.empty() || eliteProgress.hasNextShip;
                const std::string nextEliteShipLabel =
                    "Elite " + std::to_string(static_cast<int>(self->eliteShips.size()) + 1);
                const std::string progressLabel = !hasEliteData
                    ? std::string("Aucun navire elite alimente")
                    : eliteProgress.hasNextShip
                        ? ("Prochain navire : " + nextEliteShipLabel)
                        : std::string("Collection elite complete");

                drawTextAt(&self->bodyFont, formatWithDots(currentElitePoints), layout.fleetPointsPanel.x + 12.0f, layout.fleetPointsPanel.y + 8.0f, kTextGold);
                drawCentered(
                    &self->bodyFont,
                    progressLabel.c_str(),
                    SDL_FRect{layout.fleetPointsPanel.x + 128.0f, layout.fleetPointsPanel.y + 4.0f, layout.fleetPointsPanel.w - 256.0f, 18.0f},
                    kTextGold);
                drawTextAt(
                    &self->bodyFont,
                    !hasEliteData ? std::string("-") :
                    eliteProgress.hasNextShip ? formatWithDots(nextEliteTargetPoints) : std::string("MAX"),
                    layout.fleetPointsPanel.x + layout.fleetPointsPanel.w - 120.0f,
                    layout.fleetPointsPanel.y + 8.0f,
                    kTextGold);

                const SDL_FRect barTrack = SDL_FRect{
                    layout.fleetPointsPanel.x + 56.0f,
                    layout.fleetPointsPanel.y + layout.fleetPointsPanel.h - 16.0f,
                    layout.fleetPointsPanel.w - 112.0f,
                    8.0f
                };
                const SDL_FRect barFill = SDL_FRect{
                    barTrack.x,
                    barTrack.y,
                    barTrack.w * (hasEliteData ? ratio : 0.0f),
                    barTrack.h
                };
                rc2d_graphics_setColor(kScrollTrack);
                rc2d_graphics_rectangle("fill", &barTrack);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("fill", &barFill);
            }

            rc2d_graphics_setColor(kPanelFill);
            rc2d_graphics_rectangle("fill", &viewport);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &viewport);

            for (int visibleIndex = 0; visibleIndex < visibleShipCount; ++visibleIndex)
            {
                const SDL_FRect cardRect = getFleetCardRect(viewport, metrics, visibleIndex, firstRow);
                if (cardRect.w <= 0.0f || cardRect.h <= 0.0f)
                {
                    continue;
                }

                const int sourceIndex = self->getVisibleShipSourceIndexForTab(self->activeTab, visibleIndex);
                if (sourceIndex < 0 || sourceIndex >= static_cast<int>(ships->size()))
                {
                    continue;
                }

                const ShipEntry& entry = (*ships)[static_cast<std::size_t>(sourceIndex)];

                rc2d_graphics_setColor(kFieldFill);
                rc2d_graphics_rectangle("fill", &cardRect);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("line", &cardRect);

                const SDL_FRect topBand = SDL_FRect{cardRect.x, cardRect.y, cardRect.w, 24.0f};
                rc2d_graphics_setColor(kHeaderFill);
                rc2d_graphics_rectangle("fill", &topBand);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("line", &topBand);
                drawCentered(&self->smallFont, entry.name.c_str(), topBand, kTextGold);

                const float iconSize = 76.0f;
                const float contentTop = topBand.y + topBand.h;
                const float contentHeight = cardRect.h - topBand.h;

                const SDL_FRect iconRect = SDL_FRect{
                    cardRect.x + ((cardRect.w - iconSize) * 0.5f),
                    contentTop + ((contentHeight - iconSize) * 0.5f),
                    iconSize,
                    iconSize
                };
                rc2d_graphics_setColor(kPanelFill);
                rc2d_graphics_rectangle("fill", &iconRect);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("line", &iconRect);

                if (sourceIndex >= 0 && sourceIndex < static_cast<int>(shipIcons->size()))
                {
                    drawImageFit(&(*shipIcons)[static_cast<std::size_t>(sourceIndex)], iconRect);
                }
            }

            if (visibleShipCount == 0)
            {
                drawCentered(
                    &self->bodyFont,
                    eliteTab ? "Aucun navire elite acquis" : "Aucun navire special acquis",
                    viewport,
                    kTextMuted);
            }

            if (metrics.maxFirstRow > 0)
            {
                const float thumbHeight = (std::max)(
                    kMinThumbHeight,
                    (metrics.scrollTrack.h * static_cast<float>(metrics.visibleRows) / static_cast<float>((std::max)(1, metrics.totalRows))));
                const float thumbTravel = (std::max)(1.0f, metrics.scrollTrack.h - thumbHeight);
                const float ratio = static_cast<float>(firstRow) / static_cast<float>(metrics.maxFirstRow);
                const SDL_FRect thumb = SDL_FRect{
                    metrics.scrollTrack.x,
                    metrics.scrollTrack.y + (thumbTravel * ratio),
                    metrics.scrollTrack.w,
                    thumbHeight
                };

                rc2d_graphics_setColor(kScrollTrack);
                rc2d_graphics_rectangle("fill", &metrics.scrollTrack);
                rc2d_graphics_setColor(self->fleetScrollDragging || (self->fleetScrollWheelHighlightSec > 0.0f) ? kScrollThumbDragFill : kScrollThumb);
                rc2d_graphics_rectangle("fill", &thumb);
            }
        }
    }

    if (self->openPicker != AppearancePickerType::NONE)
    {
        const std::vector<AppearanceOption>* options = self->getOptionsForPicker(self->openPicker);
        const std::vector<RC2D_Image>* optionIcons = self->getIconsForPicker(self->openPicker);
        const int* selectedIndexPtr = self->getSelectedIndexForPicker(self->openPicker);
        const int selectedIndex = selectedIndexPtr != nullptr ? *selectedIndexPtr : -1;

        const int totalRows = options != nullptr ? static_cast<int>(options->size()) : 0;
        int preferredPickerRows = kPickerMaxVisibleRows;
        switch (self->openPicker)
        {
            case AppearancePickerType::REPAIR_STYLE:
            case AppearancePickerType::SPEED_STYLE:
            case AppearancePickerType::EXPLOSION_STYLE:
            case AppearancePickerType::ROCKET_STYLE:
            case AppearancePickerType::PROJECTILE_STYLE:
            case AppearancePickerType::MOVE_CLICK_STYLE:
                preferredPickerRows = kEffectPickerMaxVisibleRows;
                break;
            default:
                break;
        }

        const bool openUpward = self->openPicker == AppearancePickerType::EMOTE;
        const PickerLayout pickerLayout = buildPickerLayout(
            self->widgetRect,
            self->openPickerAnchorRect,
            totalRows,
            preferredPickerRows,
            openUpward);
        self->pickerFirstRow = (std::max)(0, (std::min)(self->pickerFirstRow, pickerLayout.maxFirstRow));

        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &pickerLayout.panel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &pickerLayout.panel);

        const int firstRow = self->pickerFirstRow;
        const int lastRow = (std::min)(pickerLayout.totalRows, firstRow + pickerLayout.visibleRows);
        for (int row = firstRow; row < lastRow; ++row)
        {
            const int visualRow = row - firstRow;
            SDL_FRect rowRect = SDL_FRect{
                pickerLayout.body.x,
                pickerLayout.body.y + (static_cast<float>(visualRow) * kPickerRowHeight),
                getPickerListRowWidth(pickerLayout),
                kPickerRowHeight
            };

            const bool isSelected = row == selectedIndex;
            rc2d_graphics_setColor(isSelected ? kSelectionFill : kFieldFill);
            rc2d_graphics_rectangle("fill", &rowRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &rowRect);

            const SDL_FRect iconRect = SDL_FRect{rowRect.x + 4.0f, rowRect.y + 4.0f, rowRect.h - 8.0f, rowRect.h - 8.0f};
            rc2d_graphics_setColor(kPanelFill);
            rc2d_graphics_rectangle("fill", &iconRect);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &iconRect);
            if (optionIcons != nullptr && row >= 0 && row < static_cast<int>(optionIcons->size()))
            {
                drawImageFit(&(*optionIcons)[static_cast<std::size_t>(row)], iconRect);
            }

            if (options != nullptr && row >= 0 && row < static_cast<int>(options->size()))
            {
                drawTextAt(
                    &self->smallFont,
                    (*options)[static_cast<std::size_t>(row)].name,
                    iconRect.x + iconRect.w + 8.0f,
                    rowRect.y + 10.0f,
                    isSelected ? kTextGold : kTextBody);
            }
        }

        if (pickerLayout.maxFirstRow > 0)
        {
            const float thumbHeight = (std::max)(
                kMinThumbHeight,
                (pickerLayout.scrollTrack.h * static_cast<float>(pickerLayout.visibleRows) / static_cast<float>((std::max)(1, pickerLayout.totalRows))));
            const float thumbTravel = (std::max)(1.0f, pickerLayout.scrollTrack.h - thumbHeight);
            const float ratio = static_cast<float>(self->pickerFirstRow) / static_cast<float>(pickerLayout.maxFirstRow);
            const SDL_FRect thumb = SDL_FRect{
                pickerLayout.scrollTrack.x,
                pickerLayout.scrollTrack.y + (thumbTravel * ratio),
                pickerLayout.scrollTrack.w,
                thumbHeight
            };

            rc2d_graphics_setColor(kScrollTrack);
            rc2d_graphics_rectangle("fill", &pickerLayout.scrollTrack);
            rc2d_graphics_setColor(self->pickerScrollDragging || (self->pickerScrollWheelHighlightSec > 0.0f) ? kScrollThumbDragFill : kScrollThumb);
            rc2d_graphics_rectangle("fill", &thumb);
        }
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}
