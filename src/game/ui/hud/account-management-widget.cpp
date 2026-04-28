#include "game/ui/hud/account-management-widget.h"
#include "game/assets/title-asset-cache.h"

#include "core/context.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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
static constexpr float kStorageItemRowHeight = 72.0f;
static constexpr float kStorageItemRowGap = 6.0f;
static constexpr float kStorageItemSidePad = 8.0f;
static constexpr float kStorageDragIconSize = 60.0f;
static constexpr float kBoardingLootRowHeight = 56.0f;
static constexpr float kBoardingLootRowGap = 6.0f;
static constexpr float kBoardingLootSidePad = 10.0f;
static constexpr float kBoardingLootIconSize = 38.0f;

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
static constexpr RC2D_Color kStorageDropReadyFill = RC2D_Color{24, 86, 62, 178};
static constexpr RC2D_Color kStorageDropReadyLine = RC2D_Color{92, 220, 144, 255};
static constexpr RC2D_Color kStorageDragGhostFill = RC2D_Color{12, 12, 14, 210};
static constexpr RC2D_Color kPopupOverlayFill = RC2D_Color{0, 0, 0, 120};

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
    SDL_FRect tabBoardingLoot;
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
    SDL_FRect boardingLootLeftPanel;
    SDL_FRect boardingLootRightPanel;
    SDL_FRect boardingLootTransferAllButton;
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

struct StorageTransferPopupLayout {
    SDL_FRect overlay;
    SDL_FRect panel;
    SDL_FRect titleBar;
    SDL_FRect icon;
    SDL_FRect quantityMinusButton;
    SDL_FRect quantityValue;
    SDL_FRect quantityPlusButton;
    SDL_FRect cancelButton;
    SDL_FRect transferButton;
};

struct BoardingLootMetrics {
    int totalRows;
    int visibleRows;
    int maxFirstRow;
    SDL_FRect leftBody;
    SDL_FRect rightBody;
    SDL_FRect scrollTrack;
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

static SDL_FRect getStorageItemRowRect(const SDL_FRect& contentRect, int visibleRow)
{
    return SDL_FRect{
        contentRect.x + kStorageItemSidePad,
        contentRect.y + kStorageItemSidePad +
            (static_cast<float>((std::max)(0, visibleRow)) * (kStorageItemRowHeight + kStorageItemRowGap)),
        (std::max)(0.0f, contentRect.w - (kStorageItemSidePad * 2.0f)),
        kStorageItemRowHeight
    };
}

static int getStorageVisibleRowCapacity(const SDL_FRect& contentRect)
{
    const float availableH = (std::max)(0.0f, contentRect.h - (kStorageItemSidePad * 2.0f));
    return (std::max)(0, static_cast<int>(std::floor((availableH + kStorageItemRowGap) / (kStorageItemRowHeight + kStorageItemRowGap))));
}

static BoardingLootMetrics buildBoardingLootMetrics(
    const SDL_FRect& leftPanel,
    const SDL_FRect& rightPanel,
    int totalRows)
{
    BoardingLootMetrics metrics{};
    metrics.totalRows = (std::max)(0, totalRows);
    metrics.leftBody = SDL_FRect{
        leftPanel.x + kBoardingLootSidePad,
        leftPanel.y + kBoardingLootSidePad,
        leftPanel.w - (kBoardingLootSidePad * 2.0f),
        leftPanel.h - (kBoardingLootSidePad * 2.0f)
    };
    metrics.rightBody = SDL_FRect{
        rightPanel.x + kBoardingLootSidePad,
        rightPanel.y + kBoardingLootSidePad,
        rightPanel.w - (kBoardingLootSidePad * 2.0f),
        rightPanel.h - (kBoardingLootSidePad * 2.0f)
    };

    const bool needsScrollProbe =
        metrics.totalRows * static_cast<int>(kBoardingLootRowHeight + kBoardingLootRowGap) >
        static_cast<int>((std::max)(metrics.leftBody.h, 0.0f) + kBoardingLootRowGap);
    if (needsScrollProbe)
    {
        const float scrollReserve = kScrollBarWidth + kScrollBarPadding;
        metrics.rightBody.w = (std::max)(0.0f, metrics.rightBody.w - scrollReserve);
    }

    const float availableH = (std::max)(0.0f, metrics.leftBody.h);
    metrics.visibleRows = (std::max)(
        1,
        static_cast<int>(std::floor((availableH + kBoardingLootRowGap) / (kBoardingLootRowHeight + kBoardingLootRowGap))));
    metrics.visibleRows = (std::min)(metrics.visibleRows, (std::max)(1, metrics.totalRows));
    metrics.maxFirstRow = (std::max)(0, metrics.totalRows - metrics.visibleRows);
    metrics.scrollTrack = SDL_FRect{
        rightPanel.x + rightPanel.w - (kScrollBarWidth + kScrollBarPadding),
        rightPanel.y + kBoardingLootSidePad,
        kScrollBarWidth,
        rightPanel.h - (kBoardingLootSidePad * 2.0f)
    };
    return metrics;
}

static SDL_FRect getBoardingLootRowRect(const SDL_FRect& bodyRect, int visibleRow)
{
    return SDL_FRect{
        bodyRect.x,
        bodyRect.y + (static_cast<float>((std::max)(0, visibleRow)) * (kBoardingLootRowHeight + kBoardingLootRowGap)),
        bodyRect.w,
        kBoardingLootRowHeight
    };
}

static StorageTransferPopupLayout buildStorageTransferPopupLayout(const SDL_FRect& widgetRect)
{
    StorageTransferPopupLayout layout{};
    layout.overlay = widgetRect;

    const float panelW = 390.0f;
    const float panelH = 260.0f;
    layout.panel = SDL_FRect{
        widgetRect.x + ((widgetRect.w - panelW) * 0.5f),
        widgetRect.y + ((widgetRect.h - panelH) * 0.5f),
        panelW,
        panelH
    };
    layout.titleBar = SDL_FRect{layout.panel.x, layout.panel.y, layout.panel.w, 34.0f};
    layout.icon = SDL_FRect{layout.panel.x + 18.0f, layout.titleBar.y + layout.titleBar.h + 18.0f, 64.0f, 64.0f};

    const float quantityY = layout.icon.y + layout.icon.h + 24.0f;
    layout.quantityMinusButton = SDL_FRect{layout.panel.x + 116.0f, quantityY, 34.0f, 34.0f};
    layout.quantityValue = SDL_FRect{
        layout.quantityMinusButton.x + layout.quantityMinusButton.w + 8.0f,
        quantityY,
        86.0f,
        34.0f
    };
    layout.quantityPlusButton = SDL_FRect{
        layout.quantityValue.x + layout.quantityValue.w + 8.0f,
        quantityY,
        34.0f,
        34.0f
    };

    const float buttonY = layout.panel.y + layout.panel.h - 50.0f;
    layout.cancelButton = SDL_FRect{layout.panel.x + 18.0f, buttonY, 150.0f, 34.0f};
    layout.transferButton = SDL_FRect{
        layout.panel.x + layout.panel.w - 168.0f,
        buttonY,
        150.0f,
        34.0f
    };

    return layout;
}

static const char* storageLocationLabel(AccountManagementWidget::StorageTabTransferLocation location)
{
    return location == AccountManagementWidget::StorageTabTransferLocation::WAREHOUSE
        ? "Entrepot"
        : "Navire";
}

static bool storageItemIdentityMatches(
    const AccountManagementWidget::StorageTabItemEntry& lhs,
    const AccountManagementWidget::StorageTabItemEntry& rhs)
{
    return lhs.category == rhs.category &&
           lhs.name == rhs.name &&
           lhs.previewAssetPath == rhs.previewAssetPath;
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
    layout.tabAccount = SDL_FRect{outer.x + 10.0f, tabY, 76.0f, tabH};
    layout.tabStorageEquipped = SDL_FRect{layout.tabAccount.x + layout.tabAccount.w + tabGap, tabY, 128.0f, tabH};
    layout.tabBoardingLoot = SDL_FRect{layout.tabStorageEquipped.x + layout.tabStorageEquipped.w + tabGap, tabY, 190.0f, tabH};
    layout.tabShipManagement = SDL_FRect{layout.tabBoardingLoot.x + layout.tabBoardingLoot.w + tabGap, tabY, 126.0f, tabH};
    layout.tabAppearance = SDL_FRect{layout.tabShipManagement.x + layout.tabShipManagement.w + tabGap, tabY, 92.0f, tabH};
    layout.tabElite = SDL_FRect{layout.tabAppearance.x + layout.tabAppearance.w + tabGap, tabY, 144.0f, tabH};
    layout.tabSpecial = SDL_FRect{layout.tabElite.x + layout.tabElite.w + tabGap, tabY, 162.0f, tabH};

    const float dragLeft = layout.tabSpecial.x + layout.tabSpecial.w + 4.0f;
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
    const float storageColW = (layout.content.w - storageGap) / 2.0f;
    const float storageDropH = 34.0f;

    layout.storageLeftPanel = SDL_FRect{layout.content.x, storagePanelY, storageColW, storagePanelH};
    layout.storageMiddlePanel = SDL_FRect{layout.storageLeftPanel.x + storageColW + storageGap, storagePanelY, storageColW, storagePanelH};

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

    const float boardingLootTopInset = 58.0f;
    const float boardingLootPanelY = layout.content.y + boardingLootTopInset;
    const float boardingLootButtonW = 236.0f;
    const float boardingLootButtonH = 32.0f;
    layout.boardingLootTransferAllButton = SDL_FRect{
        layout.content.x + layout.content.w - boardingLootButtonW,
        layout.content.y + 6.0f,
        boardingLootButtonW,
        boardingLootButtonH
    };
    layout.boardingLootLeftPanel = SDL_FRect{
        layout.content.x,
        boardingLootPanelY,
        (layout.content.w - storageGap) * 0.5f,
        (layout.content.y + layout.content.h) - boardingLootPanelY
    };
    layout.boardingLootRightPanel = SDL_FRect{
        layout.boardingLootLeftPanel.x + layout.boardingLootLeftPanel.w + storageGap,
        boardingLootPanelY,
        layout.boardingLootLeftPanel.w,
        layout.boardingLootLeftPanel.h
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
      storageDropdownArrowImage{},
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
      storageEquipmentOptionCategories{},
      storageEquipmentOptionMaxEquipped{},
      storageWarehouseItems{},
      storageEquippedItems{},
      boardingLootCurrencies{},
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
      storageWarehouseItemIcons{},
      storageEquippedItemIcons{},
      boardingLootCurrencyIcons{},
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
      storageDrag{
          false,
          StorageTabTransferLocation::WAREHOUSE,
          -1,
          0,
          StorageTabEquipmentCategory::CANNONS,
          {},
          {},
          0.0f,
          0.0f},
      storageTransferPopup{
          false,
          StorageTabTransferLocation::WAREHOUSE,
          StorageTabTransferLocation::SHIP,
          -1,
          0,
          1,
          StorageTabEquipmentCategory::CANNONS,
          {},
          {}},
      storageTabOnTransferRequested{},
      boardingLootOnTransferAllToSecureReserveRequested{},
      boardingLootFirstRow(0),
      boardingLootScrollDragging(false),
      boardingLootScrollDragOffsetY(0.0f),
      boardingLootScrollWheelHighlightSec(0.0f),
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

void AccountManagementWidget::setEliteShipsTabAcquiredShips(const std::vector<EliteShipsTabShipEntry>& ships)
{
    std::vector<InternalShipEntry> converted;
    converted.reserve(ships.size());
    for (const EliteShipsTabShipEntry& ship : ships)
    {
        converted.push_back(InternalShipEntry{ship.name, ship.previewAssetPath});
    }
    this->setShipsForCollection(ShipCollectionType::ELITE_ACQUIRED, converted);
}

void AccountManagementWidget::addEliteShipsTabAcquiredShip(const EliteShipsTabShipEntry& ship)
{
    this->addShipToCollection(ShipCollectionType::ELITE_ACQUIRED, InternalShipEntry{ship.name, ship.previewAssetPath});
}

void AccountManagementWidget::clearEliteShipsTabAcquiredShips(void)
{
    this->clearShipsForCollection(ShipCollectionType::ELITE_ACQUIRED);
}

void AccountManagementWidget::setSpecialShipsTabAcquiredShips(const std::vector<SpecialShipsTabShipEntry>& ships)
{
    std::vector<InternalShipEntry> converted;
    converted.reserve(ships.size());
    for (const SpecialShipsTabShipEntry& ship : ships)
    {
        converted.push_back(InternalShipEntry{ship.name, ship.previewAssetPath});
    }
    this->setShipsForCollection(ShipCollectionType::SPECIAL_ACQUIRED, converted);
}

void AccountManagementWidget::addSpecialShipsTabAcquiredShip(const SpecialShipsTabShipEntry& ship)
{
    this->addShipToCollection(ShipCollectionType::SPECIAL_ACQUIRED, InternalShipEntry{ship.name, ship.previewAssetPath});
}

void AccountManagementWidget::clearSpecialShipsTabAcquiredShips(void)
{
    this->clearShipsForCollection(ShipCollectionType::SPECIAL_ACQUIRED);
}

void AccountManagementWidget::setShipManagementTabBonusOptions(const std::vector<ShipManagementTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const ShipManagementTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::SHIP_BONUS, converted);
}

void AccountManagementWidget::addShipManagementTabBonusOption(const ShipManagementTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::SHIP_BONUS, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearShipManagementTabBonusOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::SHIP_BONUS);
}

void AccountManagementWidget::setAppearanceTabShipStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::SHIP_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabShipStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::SHIP_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabShipStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::SHIP_STYLE);
}

void AccountManagementWidget::setAppearanceTabRepairStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::REPAIR_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabRepairStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::REPAIR_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabRepairStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::REPAIR_STYLE);
}

void AccountManagementWidget::setAppearanceTabSpeedStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::SPEED_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabSpeedStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::SPEED_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabSpeedStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::SPEED_STYLE);
}

void AccountManagementWidget::setAppearanceTabProjectileImpactStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::PROJECTILE_IMPACT_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabProjectileImpactStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::PROJECTILE_IMPACT_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabProjectileImpactStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::PROJECTILE_IMPACT_STYLE);
}

void AccountManagementWidget::setAppearanceTabRocketStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::ROCKET_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabRocketStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::ROCKET_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabRocketStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::ROCKET_STYLE);
}

void AccountManagementWidget::setAppearanceTabProjectileStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::PROJECTILE_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabProjectileStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::PROJECTILE_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabProjectileStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::PROJECTILE_STYLE);
}

void AccountManagementWidget::setAppearanceTabMoveClickStyleOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::MOVE_CLICK_STYLE, converted);
}

void AccountManagementWidget::addAppearanceTabMoveClickStyleOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::MOVE_CLICK_STYLE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabMoveClickStyleOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::MOVE_CLICK_STYLE);
}

void AccountManagementWidget::setAppearanceTabEmoteOptions(const std::vector<AppearanceTabOptionEntry>& options)
{
    std::vector<InternalOptionEntry> converted;
    converted.reserve(options.size());
    for (const AppearanceTabOptionEntry& option : options)
    {
        converted.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    }
    this->setOptionsForCollection(OptionCollectionType::EMOTE, converted);
}

void AccountManagementWidget::addAppearanceTabEmoteOption(const AppearanceTabOptionEntry& option)
{
    this->addOptionToCollection(OptionCollectionType::EMOTE, InternalOptionEntry{option.name, option.previewAssetPath});
}

void AccountManagementWidget::clearAppearanceTabEmoteOptions(void)
{
    this->clearOptionsForCollection(OptionCollectionType::EMOTE);
}

void AccountManagementWidget::clearStorageTabEquipmentOptions(void)
{
    this->storageEquipmentOptions.clear();
    this->storageEquipmentOptionCategories.clear();
    this->storageEquipmentOptionMaxEquipped.clear();
    this->syncOptionCollectionState(OptionCollectionType::STORAGE_EQUIPMENT);
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadAppearanceIcons();
    }
}

void AccountManagementWidget::setStorageTabEquipmentCategoryOptions(
    const std::vector<StorageTabEquipmentOptionEntry>& options)
{
    this->storageEquipmentOptions.clear();
    this->storageEquipmentOptionCategories.clear();
    this->storageEquipmentOptionMaxEquipped.clear();
    this->storageEquipmentOptions.reserve(options.size());
    this->storageEquipmentOptionCategories.reserve(options.size());
    this->storageEquipmentOptionMaxEquipped.reserve(options.size());

    for (const StorageTabEquipmentOptionEntry& option : options)
    {
        this->storageEquipmentOptions.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
        this->storageEquipmentOptionCategories.push_back(option.category);
        this->storageEquipmentOptionMaxEquipped.push_back((std::max)(0, option.maxEquippedOnShip));
    }

    this->syncOptionCollectionState(OptionCollectionType::STORAGE_EQUIPMENT);
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadAppearanceIcons();
    }
}

void AccountManagementWidget::addStorageTabEquipmentCategoryOption(
    const StorageTabEquipmentOptionEntry& option)
{
    this->storageEquipmentOptions.push_back(InternalOptionEntry{option.name, option.previewAssetPath});
    this->storageEquipmentOptionCategories.push_back(option.category);
    this->storageEquipmentOptionMaxEquipped.push_back((std::max)(0, option.maxEquippedOnShip));
    this->syncOptionCollectionState(OptionCollectionType::STORAGE_EQUIPMENT);

    if (this->resourcesLoaded)
    {
        this->loadAppearanceIcons();
    }
}

void AccountManagementWidget::setStorageTabWarehouseItems(const std::vector<StorageTabItemEntry>& items)
{
    this->storageWarehouseItems = items;
    for (StorageTabItemEntry& item : this->storageWarehouseItems)
    {
        item.quantity = (std::max)(0, item.quantity);
    }
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }
}

void AccountManagementWidget::addStorageTabWarehouseItem(const StorageTabItemEntry& item)
{
    StorageTabItemEntry sanitized = item;
    sanitized.quantity = (std::max)(0, sanitized.quantity);
    this->storageWarehouseItems.push_back(sanitized);

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }
}

void AccountManagementWidget::clearStorageTabWarehouseItems(void)
{
    this->storageWarehouseItems.clear();
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }
}

void AccountManagementWidget::setStorageTabEquippedItems(const std::vector<StorageTabItemEntry>& items)
{
    this->storageEquippedItems = items;
    for (StorageTabItemEntry& item : this->storageEquippedItems)
    {
        item.quantity = (std::max)(0, item.quantity);
    }
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }
}

void AccountManagementWidget::addStorageTabEquippedItem(const StorageTabItemEntry& item)
{
    StorageTabItemEntry sanitized = item;
    sanitized.quantity = (std::max)(0, sanitized.quantity);
    this->storageEquippedItems.push_back(sanitized);

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }
}

void AccountManagementWidget::clearStorageTabEquippedItems(void)
{
    this->storageEquippedItems.clear();
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }
}

void AccountManagementWidget::setStorageTabOnTransferRequested(StorageTabTransferCallback callback)
{
    this->storageTabOnTransferRequested = std::move(callback);
}

void AccountManagementWidget::setBoardingLootManagementTabCurrencies(
    const std::vector<BoardingLootCurrencyEntry>& currencies)
{
    this->boardingLootCurrencies = currencies;
    for (BoardingLootCurrencyEntry& currency : this->boardingLootCurrencies)
    {
        currency.shipQuantity = (std::max)(0, currency.shipQuantity);
        currency.secureReserveQuantity = (std::max)(0, currency.secureReserveQuantity);
        currency.minQuantityToSecureReserve = (std::max)(0, currency.minQuantityToSecureReserve);
    }
    this->boardingLootFirstRow = 0;
    if (this->resourcesLoaded)
    {
        this->loadBoardingLootCurrencyIcons();
    }
}

void AccountManagementWidget::addBoardingLootManagementTabCurrency(const BoardingLootCurrencyEntry& currency)
{
    BoardingLootCurrencyEntry sanitized = currency;
    sanitized.shipQuantity = (std::max)(0, sanitized.shipQuantity);
    sanitized.secureReserveQuantity = (std::max)(0, sanitized.secureReserveQuantity);
    sanitized.minQuantityToSecureReserve = (std::max)(0, sanitized.minQuantityToSecureReserve);
    this->boardingLootCurrencies.push_back(sanitized);
    if (this->resourcesLoaded)
    {
        this->loadBoardingLootCurrencyIcons();
    }
}

void AccountManagementWidget::clearBoardingLootManagementTabCurrencies(void)
{
    this->boardingLootCurrencies.clear();
    this->boardingLootFirstRow = 0;
    this->boardingLootScrollDragging = false;
    this->clearIcons(this->boardingLootCurrencyIcons);
}

void AccountManagementWidget::setBoardingLootManagementTabOnTransferAllToSecureReserveRequested(
    BoardingLootTransferAllCallback callback)
{
    this->boardingLootOnTransferAllToSecureReserveRequested = std::move(callback);
}

void AccountManagementWidget::setShipsForCollection(
    ShipCollectionType collection,
    const std::vector<InternalShipEntry>& ships)
{
    std::vector<InternalShipEntry>* target = this->getShipsForCollection(collection);
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

void AccountManagementWidget::addShipToCollection(ShipCollectionType collection, const InternalShipEntry& ship)
{
    std::vector<InternalShipEntry>* target = this->getShipsForCollection(collection);
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
    std::vector<InternalShipEntry>* target = this->getShipsForCollection(collection);
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
    const std::vector<InternalOptionEntry>& options)
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
    const InternalOptionEntry& option)
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

void AccountManagementWidget::setAccountTabEliteProgressData(const AccountTabEliteProgressData& progressData)
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

void AccountManagementWidget::setAccountTabElitePointsCurrent(int points)
{
    this->eliteProgressData.currentPoints = (std::max)(0, points);
    if (this->eliteProgressData.hasNextShip)
    {
        this->eliteProgressData.nextShipPointsRequired =
            (std::max)(this->eliteProgressData.currentPoints, this->eliteProgressData.nextShipPointsRequired);
    }
}

void AccountManagementWidget::setAccountTabPlayerIdentifier(const std::string& value)
{
    this->playerIdentifier = value;
}

void AccountManagementWidget::setAccountTabPirateSince(const std::string& value)
{
    this->pirateSinceText = value;
}

void AccountManagementWidget::setAccountTabPlayerLevel(int level)
{
    this->playerLevel = (std::max)(0, level);
}

void AccountManagementWidget::setAccountTabExperiencePointsCurrent(int points)
{
    this->experiencePointsCurrent = (std::max)(0, points);
}

void AccountManagementWidget::setAccountTabCombatPointsCurrent(int points)
{
    this->combatPointsCurrent = (std::max)(0, points);
}

void AccountManagementWidget::setAccountTabPremiumSince(const std::string& value)
{
    this->premiumSinceText = value;
}

void AccountManagementWidget::setAccountTabProfileName(const std::string& value)
{
    this->profileName = clampProfileNameToUiLimit(value);
    this->profileCursorIndex = this->profileName.size();
}

AccountManagementWidget::AccountTabEliteProgressData AccountManagementWidget::getAccountTabEliteProgress(void) const
{
    AccountTabEliteProgressData data = this->eliteProgressData;
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
    this->clearIcons(this->storageWarehouseItemIcons);
    this->clearIcons(this->storageEquippedItemIcons);
    this->clearIcons(this->boardingLootCurrencyIcons);

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
    this->storageEquipmentOptionCategories.clear();
    this->storageEquipmentOptionMaxEquipped.clear();
    this->storageWarehouseItems.clear();
    this->storageEquippedItems.clear();
    this->boardingLootCurrencies.clear();

    this->playerIdentifier.clear();
    this->pirateSinceText.clear();
    this->playerLevel = 0;
    this->experiencePointsCurrent = 0;
    this->eliteProgressData = AccountTabEliteProgressData{0, 0, false};
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
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();
    this->storageTabOnTransferRequested = nullptr;
    this->boardingLootOnTransferAllToSecureReserveRequested = nullptr;
    this->boardingLootFirstRow = 0;
    this->boardingLootScrollDragging = false;
    this->boardingLootScrollDragOffsetY = 0.0f;
    this->boardingLootScrollWheelHighlightSec = 0.0f;
    this->profileName.clear();
    this->profileCursorIndex = 0;
}

std::vector<AccountManagementWidget::InternalShipEntry>* AccountManagementWidget::getShipsForCollection(
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

const std::vector<AccountManagementWidget::InternalShipEntry>* AccountManagementWidget::getShipsForCollection(
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
    std::vector<InternalShipEntry>* ships = this->getShipsForCollection(collection);
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

    if (collection == OptionCollectionType::STORAGE_EQUIPMENT &&
        this->storageEquipmentOptionCategories.size() != this->storageEquipmentOptions.size())
    {
        this->storageEquipmentOptionCategories.resize(
            this->storageEquipmentOptions.size(),
            StorageTabEquipmentCategory::CANNONS);
    }
    if (collection == OptionCollectionType::STORAGE_EQUIPMENT &&
        this->storageEquipmentOptionMaxEquipped.size() != this->storageEquipmentOptions.size())
    {
        this->storageEquipmentOptionMaxEquipped.resize(this->storageEquipmentOptions.size(), 0);
    }

    if (this->openPicker != AppearancePickerType::NONE &&
        this->getCollectionForPicker(this->openPicker) == collection &&
        options->empty())
    {
        this->closeAppearancePicker();
    }
}

AccountManagementWidget::StorageTabEquipmentCategory AccountManagementWidget::getSelectedStorageTabEquipmentCategory(void) const
{
    if (this->storageEquipmentOptionCategories.empty())
    {
        return StorageTabEquipmentCategory::CANNONS;
    }

    const int index = (std::max)(
        0,
        (std::min)(this->selectedStorageEquipmentOption, static_cast<int>(this->storageEquipmentOptionCategories.size()) - 1));
    return this->storageEquipmentOptionCategories[static_cast<std::size_t>(index)];
}

int AccountManagementWidget::getStorageEquippedQuantityForCategory(StorageTabEquipmentCategory category) const
{
    int totalQuantity = 0;
    for (const StorageTabItemEntry& item : this->storageEquippedItems)
    {
        if (item.category == category)
        {
            totalQuantity += (std::max)(0, item.quantity);
        }
    }
    return totalQuantity;
}

int AccountManagementWidget::getStorageMaxEquippedForCategory(StorageTabEquipmentCategory category) const
{
    int maxEquipped = 0;
    const std::size_t optionCount = (std::min)(
        this->storageEquipmentOptionCategories.size(),
        this->storageEquipmentOptionMaxEquipped.size());
    for (std::size_t i = 0; i < optionCount; ++i)
    {
        if (this->storageEquipmentOptionCategories[i] == category)
        {
            maxEquipped = (std::max)(maxEquipped, (std::max)(0, this->storageEquipmentOptionMaxEquipped[i]));
        }
    }
    return maxEquipped;
}

int AccountManagementWidget::getStorageRemainingEquipCapacityForCategory(StorageTabEquipmentCategory category) const
{
    const int maxEquipped = this->getStorageMaxEquippedForCategory(category);
    if (maxEquipped <= 0)
    {
        return (std::numeric_limits<int>::max)();
    }

    return (std::max)(0, maxEquipped - this->getStorageEquippedQuantityForCategory(category));
}

bool AccountManagementWidget::isStorageWarehouseItemDisabledForEquip(const StorageTabItemEntry& item) const
{
    return item.quantity <= 0 || this->getStorageRemainingEquipCapacityForCategory(item.category) <= 0;
}

bool AccountManagementWidget::isStorageItemVisibleForSelectedCategory(const StorageTabItemEntry& item) const
{
    return item.category == this->getSelectedStorageTabEquipmentCategory();
}

std::vector<AccountManagementWidget::StorageTabItemEntry>* AccountManagementWidget::getStorageItemsForLocation(
    StorageTabTransferLocation location)
{
    return location == StorageTabTransferLocation::WAREHOUSE
        ? &this->storageWarehouseItems
        : &this->storageEquippedItems;
}

const std::vector<AccountManagementWidget::StorageTabItemEntry>* AccountManagementWidget::getStorageItemsForLocation(
    StorageTabTransferLocation location) const
{
    return location == StorageTabTransferLocation::WAREHOUSE
        ? &this->storageWarehouseItems
        : &this->storageEquippedItems;
}

std::vector<RC2D_Image>* AccountManagementWidget::getStorageItemIconsForLocation(StorageTabTransferLocation location)
{
    return location == StorageTabTransferLocation::WAREHOUSE
        ? &this->storageWarehouseItemIcons
        : &this->storageEquippedItemIcons;
}

const std::vector<RC2D_Image>* AccountManagementWidget::getStorageItemIconsForLocation(StorageTabTransferLocation location) const
{
    return location == StorageTabTransferLocation::WAREHOUSE
        ? &this->storageWarehouseItemIcons
        : &this->storageEquippedItemIcons;
}

int AccountManagementWidget::hitTestStorageItem(
    StorageTabTransferLocation location,
    const SDL_FRect& contentRect,
    float x,
    float y,
    bool requireDraggable) const
{
    if (!isPointInRect(x, y, contentRect))
    {
        return -1;
    }

    const std::vector<StorageTabItemEntry>* items = this->getStorageItemsForLocation(location);
    if (items == nullptr)
    {
        return -1;
    }

    const int rowCapacity = getStorageVisibleRowCapacity(contentRect);
    int visibleRow = 0;
    for (int sourceIndex = 0; sourceIndex < static_cast<int>(items->size()); ++sourceIndex)
    {
        const StorageTabItemEntry& item = (*items)[static_cast<std::size_t>(sourceIndex)];
        if (!this->isStorageItemVisibleForSelectedCategory(item) || item.quantity <= 0)
        {
            continue;
        }

        if (visibleRow >= rowCapacity)
        {
            break;
        }

        const SDL_FRect rowRect = getStorageItemRowRect(contentRect, visibleRow);
        if (isPointInRect(x, y, rowRect))
        {
            if (requireDraggable &&
                location == StorageTabTransferLocation::WAREHOUSE &&
                this->isStorageWarehouseItemDisabledForEquip(item))
            {
                return -1;
            }
            return sourceIndex;
        }

        ++visibleRow;
    }

    return -1;
}

void AccountManagementWidget::openStorageTransferPopupFromDrag(StorageTabTransferLocation target)
{
    if (!this->storageDrag.active)
    {
        return;
    }

    int maxQuantity = (std::max)(1, this->storageDrag.maxQuantity);
    if (this->storageDrag.source == StorageTabTransferLocation::WAREHOUSE &&
        target == StorageTabTransferLocation::SHIP)
    {
        maxQuantity = (std::min)(
            maxQuantity,
            this->getStorageRemainingEquipCapacityForCategory(this->storageDrag.category));
    }
    if (maxQuantity <= 0)
    {
        this->cancelStorageDrag();
        return;
    }

    this->storageTransferPopup.open = true;
    this->storageTransferPopup.source = this->storageDrag.source;
    this->storageTransferPopup.target = target;
    this->storageTransferPopup.sourceIndex = this->storageDrag.sourceIndex;
    this->storageTransferPopup.maxQuantity = maxQuantity;
    this->storageTransferPopup.quantity = this->storageTransferPopup.maxQuantity;
    this->storageTransferPopup.category = this->storageDrag.category;
    this->storageTransferPopup.itemName = this->storageDrag.itemName;
    this->storageTransferPopup.previewAssetPath = this->storageDrag.previewAssetPath;
    this->cancelStorageDrag();
}

void AccountManagementWidget::closeStorageTransferPopup(void)
{
    this->storageTransferPopup = StorageTransferPopupState{
        false,
        StorageTabTransferLocation::WAREHOUSE,
        StorageTabTransferLocation::SHIP,
        -1,
        0,
        1,
        StorageTabEquipmentCategory::CANNONS,
        {},
        {}
    };
}

void AccountManagementWidget::cancelStorageDrag(void)
{
    this->storageDrag = StorageDragState{
        false,
        StorageTabTransferLocation::WAREHOUSE,
        -1,
        0,
        StorageTabEquipmentCategory::CANNONS,
        {},
        {},
        0.0f,
        0.0f
    };
}

void AccountManagementWidget::applyStorageTransferPopup(void)
{
    if (!this->storageTransferPopup.open)
    {
        return;
    }

    std::vector<StorageTabItemEntry>* sourceItems = this->getStorageItemsForLocation(this->storageTransferPopup.source);
    std::vector<StorageTabItemEntry>* targetItems = this->getStorageItemsForLocation(this->storageTransferPopup.target);
    if (sourceItems == nullptr ||
        targetItems == nullptr ||
        this->storageTransferPopup.sourceIndex < 0 ||
        this->storageTransferPopup.sourceIndex >= static_cast<int>(sourceItems->size()))
    {
        this->closeStorageTransferPopup();
        return;
    }

    StorageTabItemEntry movedItem = (*sourceItems)[static_cast<std::size_t>(this->storageTransferPopup.sourceIndex)];
    const int movableQuantity = (std::max)(0, movedItem.quantity);
    if (movableQuantity <= 0)
    {
        this->closeStorageTransferPopup();
        return;
    }

    int maxQuantity = (std::min)(this->storageTransferPopup.maxQuantity, movableQuantity);
    if (this->storageTransferPopup.source == StorageTabTransferLocation::WAREHOUSE &&
        this->storageTransferPopup.target == StorageTabTransferLocation::SHIP)
    {
        maxQuantity = (std::min)(
            maxQuantity,
            this->getStorageRemainingEquipCapacityForCategory(movedItem.category));
    }
    if (maxQuantity <= 0)
    {
        this->closeStorageTransferPopup();
        return;
    }

    const int quantity = (std::max)(1, (std::min)(this->storageTransferPopup.quantity, maxQuantity));

    movedItem.quantity = quantity;
    (*sourceItems)[static_cast<std::size_t>(this->storageTransferPopup.sourceIndex)].quantity -= quantity;
    if ((*sourceItems)[static_cast<std::size_t>(this->storageTransferPopup.sourceIndex)].quantity <= 0)
    {
        sourceItems->erase(sourceItems->begin() + this->storageTransferPopup.sourceIndex);
    }

    auto targetIt = std::find_if(
        targetItems->begin(),
        targetItems->end(),
        [&movedItem](const StorageTabItemEntry& entry) {
            return storageItemIdentityMatches(entry, movedItem);
        });
    if (targetIt != targetItems->end())
    {
        targetIt->quantity += quantity;
    }
    else
    {
        targetItems->push_back(movedItem);
    }

    StorageTabTransferRequest request{};
    request.source = this->storageTransferPopup.source;
    request.target = this->storageTransferPopup.target;
    request.item = movedItem;
    request.quantity = quantity;

    this->closeStorageTransferPopup();

    if (this->resourcesLoaded)
    {
        this->loadStorageItemIcons();
    }

    if (this->storageTabOnTransferRequested)
    {
        this->storageTabOnTransferRequested(request);
    }
}

void AccountManagementWidget::transferAllBoardingLootToSecureReserve(void)
{
    BoardingLootTransferAllToSecureReserveRequest request{};
    for (BoardingLootCurrencyEntry& currency : this->boardingLootCurrencies)
    {
        const int quantity = (std::max)(0, currency.shipQuantity);
        const int minQuantity = (std::max)(0, currency.minQuantityToSecureReserve);
        if (currency.forceStoreOnShip || quantity <= 0 || quantity < minQuantity)
        {
            continue;
        }

        currency.shipQuantity = 0;
        currency.secureReserveQuantity = (std::max)(0, currency.secureReserveQuantity) + quantity;

        BoardingLootTransferEntry moved{};
        moved.type = currency.type;
        moved.quantity = quantity;
        request.currencies.push_back(moved);
    }

    if (!request.currencies.empty() && this->boardingLootOnTransferAllToSecureReserveRequested)
    {
        this->boardingLootOnTransferAllToSecureReserveRequested(request);
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
    const std::vector<InternalShipEntry>* ships = this->getShipsForTab(tab);
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

    const std::vector<InternalShipEntry>* ships = this->getShipsForTab(tab);
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
    for (const InternalShipEntry& entry : this->eliteShips)
    {
        this->eliteShipIcons.push_back(loadImageFromTitleOrEmpty(entry.previewAssetPath));
    }

    this->specialShipIcons.reserve(this->specialShips.size());
    for (const InternalShipEntry& entry : this->specialShips)
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

void AccountManagementWidget::loadStorageItemIcons(void)
{
    this->clearIcons(this->storageWarehouseItemIcons);
    this->clearIcons(this->storageEquippedItemIcons);

    this->storageWarehouseItemIcons.reserve(this->storageWarehouseItems.size());
    for (const StorageTabItemEntry& item : this->storageWarehouseItems)
    {
        this->storageWarehouseItemIcons.push_back(loadImageFromTitleOrEmpty(item.previewAssetPath));
    }

    this->storageEquippedItemIcons.reserve(this->storageEquippedItems.size());
    for (const StorageTabItemEntry& item : this->storageEquippedItems)
    {
        this->storageEquippedItemIcons.push_back(loadImageFromTitleOrEmpty(item.previewAssetPath));
    }
}

void AccountManagementWidget::loadBoardingLootCurrencyIcons(void)
{
    this->clearIcons(this->boardingLootCurrencyIcons);

    this->boardingLootCurrencyIcons.reserve(this->boardingLootCurrencies.size());
    for (const BoardingLootCurrencyEntry& currency : this->boardingLootCurrencies)
    {
        this->boardingLootCurrencyIcons.push_back(loadImageFromTitleOrEmpty(currency.previewAssetPath));
    }
}

std::vector<AccountManagementWidget::InternalShipEntry>* AccountManagementWidget::getShipsForTab(ActiveTab tab)
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

const std::vector<AccountManagementWidget::InternalShipEntry>* AccountManagementWidget::getShipsForTab(ActiveTab tab) const
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
    this->storageDropdownArrowImage =
        LoadStorageImage("assets/images/ui-scene-game/icon-arrowdown.png", RC2D_STORAGE_TITLE);
    if (this->storageDropdownArrowImage.sdl_texture == nullptr)
    {
        RC2D_log(RC2D_LOG_WARN, "AccountManagementWidget: echec chargement icon-arrowdown.png");
    }
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
    this->loadStorageItemIcons();
    this->loadBoardingLootCurrencyIcons();

    this->closeAppearancePicker();
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();
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
    this->clearIcons(this->storageWarehouseItemIcons);
    this->clearIcons(this->storageEquippedItemIcons);
    this->clearIcons(this->boardingLootCurrencyIcons);
    ResetStorageImageRef(&this->storageDropdownArrowImage);
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

    if (this->pickerScrollDragging || this->fleetScrollDragging || this->boardingLootScrollDragging)
    {
        return HudCursorType::RESIZE_VERTICAL;
    }
    if (this->widgetDragging)
    {
        return HudCursorType::MOVE;
    }
    if (this->storageDrag.active)
    {
        return HudCursorType::POINTER;
    }

    const SDL_FRect baseRect = getWidgetRectFromGameScreen();
    const SDL_FRect currentRect = SDL_FRect{
        baseRect.x + this->widgetOffsetX,
        baseRect.y + this->widgetOffsetY,
        baseRect.w,
        baseRect.h
    };
    const WidgetLayout layout = buildLayout(currentRect);

    if (this->storageTransferPopup.open)
    {
        const StorageTransferPopupLayout popupLayout = buildStorageTransferPopupLayout(currentRect);
        if (isPointInRect(x, y, popupLayout.cancelButton) ||
            isPointInRect(x, y, popupLayout.transferButton) ||
            isPointInRect(x, y, popupLayout.quantityMinusButton) ||
            isPointInRect(x, y, popupLayout.quantityPlusButton))
        {
            return HudCursorType::POINTER;
        }
        return isPointInRect(x, y, popupLayout.panel)
            ? HudCursorType::DEFAULT
            : HudCursorType::DEFAULT;
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
        isPointInRect(x, y, layout.tabStorageEquipped) ||
        isPointInRect(x, y, layout.tabBoardingLoot))
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
        if (this->hitTestStorageItem(StorageTabTransferLocation::WAREHOUSE, layout.storageLeftContent, x, y, true) >= 0 ||
            this->hitTestStorageItem(StorageTabTransferLocation::SHIP, layout.storageMiddleContent, x, y) >= 0)
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == ActiveTab::BOARDING_LOOT)
    {
        const BoardingLootMetrics metrics = buildBoardingLootMetrics(
            layout.boardingLootLeftPanel,
            layout.boardingLootRightPanel,
            static_cast<int>(this->boardingLootCurrencies.size()));
        if (metrics.maxFirstRow > 0 && isPointInRect(x, y, metrics.scrollTrack))
        {
            return HudCursorType::RESIZE_VERTICAL;
        }
        if (isPointInRect(x, y, layout.boardingLootTransferAllButton))
        {
            return HudCursorType::POINTER;
        }
    }
    else if (this->activeTab == ActiveTab::ELITE_SHIPS || this->activeTab == ActiveTab::SPECIAL_SHIPS)
    {
        const std::vector<InternalShipEntry>* ships = this->getShipsForTab(this->activeTab);
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
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();
}

void AccountManagementWidget::openShipManagement(void)
{
    this->visible = true;
    this->activeTab = ActiveTab::SHIP_MANAGEMENT;
    this->widgetDragging = false;
    this->fleetScrollDragging = false;
    this->pickerScrollDragging = false;
    this->boardingLootScrollDragging = false;
    this->clearFocus();
    this->closeAppearancePicker();
    this->cancelStorageDrag();
}

void AccountManagementWidget::hide(void)
{
    this->visible = false;
    this->widgetDragging = false;
    this->fleetScrollDragging = false;
    this->pickerScrollDragging = false;
    this->boardingLootScrollDragging = false;
    this->fleetScrollWheelHighlightSec = 0.0f;
    this->boardingLootScrollWheelHighlightSec = 0.0f;
    this->clearFocus();
    this->closeAppearancePicker();
    this->cancelStorageDrag();
    this->closeStorageTransferPopup();
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
    if (this->boardingLootScrollWheelHighlightSec > 0.0f)
    {
        this->boardingLootScrollWheelHighlightSec -= dtF;
        if (this->boardingLootScrollWheelHighlightSec < 0.0f)
        {
            this->boardingLootScrollWheelHighlightSec = 0.0f;
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
    const bool leftMouseDown = rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT);

    if (this->storageDrag.active)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);

        const StorageTabTransferLocation targetLocation =
            this->storageDrag.source == StorageTabTransferLocation::WAREHOUSE
                ? StorageTabTransferLocation::SHIP
                : StorageTabTransferLocation::WAREHOUSE;
        const SDL_FRect targetRect =
            targetLocation == StorageTabTransferLocation::SHIP
                ? layout.storageMiddleContent
                : layout.storageLeftContent;
        const bool validDrop =
            this->visible &&
            this->activeTab == ActiveTab::STORAGE_EQUIPPED &&
            isPointInRect(mouseX, mouseY, targetRect) &&
            (this->storageDrag.source != StorageTabTransferLocation::WAREHOUSE ||
                targetLocation != StorageTabTransferLocation::SHIP ||
                this->getStorageRemainingEquipCapacityForCategory(this->storageDrag.category) > 0);

        if (!leftMouseDown)
        {
            if (validDrop)
            {
                this->openStorageTransferPopupFromDrag(targetLocation);
            }
            else
            {
                this->cancelStorageDrag();
            }
        }
        return;
    }

    if (!this->widgetDragging &&
        !this->fleetScrollDragging &&
        !this->pickerScrollDragging &&
        !this->boardingLootScrollDragging)
    {
        return;
    }

    if (!leftMouseDown)
    {
        this->widgetDragging = false;
        this->fleetScrollDragging = false;
        this->pickerScrollDragging = false;
        this->boardingLootScrollDragging = false;
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

    if (this->boardingLootScrollDragging)
    {
        const BoardingLootMetrics metrics = buildBoardingLootMetrics(
            layout.boardingLootLeftPanel,
            layout.boardingLootRightPanel,
            static_cast<int>(this->boardingLootCurrencies.size()));
        if (this->activeTab != ActiveTab::BOARDING_LOOT || metrics.maxFirstRow <= 0)
        {
            this->boardingLootFirstRow = 0;
            this->boardingLootScrollDragging = false;
            return;
        }

        const float thumbHeight = (std::max)(
            kMinThumbHeight,
            (metrics.scrollTrack.h * static_cast<float>(metrics.visibleRows) /
                static_cast<float>((std::max)(1, metrics.totalRows))));
        const float thumbTravel = (std::max)(1.0f, metrics.scrollTrack.h - thumbHeight);

        float mx = 0.0f;
        float my = 0.0f;
        getMouseRenderPosition(&mx, &my);
        (void)mx;

        const float thumbTop = clampf(
            my - this->boardingLootScrollDragOffsetY,
            metrics.scrollTrack.y,
            metrics.scrollTrack.y + thumbTravel);
        const float t = (thumbTop - metrics.scrollTrack.y) / thumbTravel;
        this->boardingLootFirstRow = static_cast<int>(t * static_cast<float>(metrics.maxFirstRow) + 0.5f);
        this->boardingLootFirstRow = (std::max)(0, (std::min)(this->boardingLootFirstRow, metrics.maxFirstRow));
        return;
    }

    if (this->fleetScrollDragging)
    {
        std::vector<InternalShipEntry>* ships = this->getShipsForTab(this->activeTab);
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

    if (this->storageTransferPopup.open)
    {
        const StorageTransferPopupLayout popupLayout = buildStorageTransferPopupLayout(this->widgetRect);
        if (isPointInRect(x, y, popupLayout.cancelButton))
        {
            this->closeStorageTransferPopup();
            return true;
        }
        if (isPointInRect(x, y, popupLayout.transferButton))
        {
            this->applyStorageTransferPopup();
            return true;
        }
        if (isPointInRect(x, y, popupLayout.quantityMinusButton))
        {
            this->storageTransferPopup.quantity = (std::max)(1, this->storageTransferPopup.quantity - 1);
            return true;
        }
        if (isPointInRect(x, y, popupLayout.quantityPlusButton))
        {
            this->storageTransferPopup.quantity =
                (std::min)(this->storageTransferPopup.maxQuantity, this->storageTransferPopup.quantity + 1);
            return true;
        }
        return true;
    }

    if (isPointInRect(x, y, layout.closeButton))
    {
        this->visible = false;
        this->widgetDragging = false;
        this->fleetScrollDragging = false;
        this->pickerScrollDragging = false;
        this->cancelStorageDrag();
        this->closeStorageTransferPopup();
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabAccount))
    {
        this->activeTab = ActiveTab::ACCOUNT;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabStorageEquipped))
    {
        this->activeTab = ActiveTab::STORAGE_EQUIPPED;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabBoardingLoot))
    {
        this->activeTab = ActiveTab::BOARDING_LOOT;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabShipManagement))
    {
        this->activeTab = ActiveTab::SHIP_MANAGEMENT;
        this->clearAllFocus();
        return true;
    }

    if (isPointInRect(x, y, layout.tabAppearance))
    {
        this->activeTab = ActiveTab::APPEARANCE;
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

        const int warehouseSourceIndex =
            this->hitTestStorageItem(StorageTabTransferLocation::WAREHOUSE, layout.storageLeftContent, x, y, true);
        if (warehouseSourceIndex >= 0)
        {
            const std::vector<StorageTabItemEntry>* sourceItems =
                this->getStorageItemsForLocation(StorageTabTransferLocation::WAREHOUSE);
            if (sourceItems != nullptr && warehouseSourceIndex < static_cast<int>(sourceItems->size()))
            {
                const StorageTabItemEntry& item = (*sourceItems)[static_cast<std::size_t>(warehouseSourceIndex)];
                const int maxTransferQuantity = (std::min)(
                    (std::max)(1, item.quantity),
                    this->getStorageRemainingEquipCapacityForCategory(item.category));
                if (maxTransferQuantity <= 0)
                {
                    this->closeAppearancePicker();
                    return true;
                }
                this->storageDrag = StorageDragState{
                    true,
                    StorageTabTransferLocation::WAREHOUSE,
                    warehouseSourceIndex,
                    maxTransferQuantity,
                    item.category,
                    item.name,
                    item.previewAssetPath,
                    x,
                    y
                };
                this->closeAppearancePicker();
                return true;
            }
        }

        const int shipSourceIndex =
            this->hitTestStorageItem(StorageTabTransferLocation::SHIP, layout.storageMiddleContent, x, y);
        if (shipSourceIndex >= 0)
        {
            const std::vector<StorageTabItemEntry>* sourceItems =
                this->getStorageItemsForLocation(StorageTabTransferLocation::SHIP);
            if (sourceItems != nullptr && shipSourceIndex < static_cast<int>(sourceItems->size()))
            {
                const StorageTabItemEntry& item = (*sourceItems)[static_cast<std::size_t>(shipSourceIndex)];
                this->storageDrag = StorageDragState{
                    true,
                    StorageTabTransferLocation::SHIP,
                    shipSourceIndex,
                    (std::max)(1, item.quantity),
                    item.category,
                    item.name,
                    item.previewAssetPath,
                    x,
                    y
                };
                this->closeAppearancePicker();
                return true;
            }
        }

        this->closeAppearancePicker();
        return true;
    }

    if (this->activeTab == ActiveTab::BOARDING_LOOT)
    {
        const BoardingLootMetrics metrics = buildBoardingLootMetrics(
            layout.boardingLootLeftPanel,
            layout.boardingLootRightPanel,
            static_cast<int>(this->boardingLootCurrencies.size()));
        this->boardingLootFirstRow = (std::max)(0, (std::min)(this->boardingLootFirstRow, metrics.maxFirstRow));
        if (metrics.maxFirstRow > 0 && isPointInRect(x, y, metrics.scrollTrack))
        {
            const float thumbHeight = (std::max)(
                kMinThumbHeight,
                (metrics.scrollTrack.h * static_cast<float>(metrics.visibleRows) /
                    static_cast<float>((std::max)(1, metrics.totalRows))));
            const float thumbTravel = (std::max)(1.0f, metrics.scrollTrack.h - thumbHeight);
            const float ratio =
                static_cast<float>(this->boardingLootFirstRow) /
                static_cast<float>((std::max)(1, metrics.maxFirstRow));
            const float thumbY = metrics.scrollTrack.y + (thumbTravel * ratio);
            const SDL_FRect thumb = SDL_FRect{metrics.scrollTrack.x, thumbY, metrics.scrollTrack.w, thumbHeight};

            this->boardingLootScrollDragging = true;
            this->fleetScrollDragging = false;
            this->pickerScrollDragging = false;
            this->widgetDragging = false;

            if (isPointInRect(x, y, thumb))
            {
                this->boardingLootScrollDragOffsetY = y - thumb.y;
            }
            else
            {
                this->boardingLootScrollDragOffsetY = thumbHeight * 0.5f;
                const float thumbTop = clampf(
                    y - this->boardingLootScrollDragOffsetY,
                    metrics.scrollTrack.y,
                    metrics.scrollTrack.y + thumbTravel);
                const float t = (thumbTop - metrics.scrollTrack.y) / thumbTravel;
                this->boardingLootFirstRow = static_cast<int>(t * static_cast<float>(metrics.maxFirstRow) + 0.5f);
                this->boardingLootFirstRow = (std::max)(0, (std::min)(this->boardingLootFirstRow, metrics.maxFirstRow));
            }
            return true;
        }

        if (isPointInRect(x, y, layout.boardingLootTransferAllButton))
        {
            this->transferAllBoardingLootToSecureReserve();
        }
        this->closeAppearancePicker();
        return true;
    }

    if (this->activeTab != ActiveTab::ELITE_SHIPS && this->activeTab != ActiveTab::SPECIAL_SHIPS)
    {
        return true;
    }

    std::vector<InternalShipEntry>* ships = this->getShipsForTab(this->activeTab);
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

    if (this->activeTab == ActiveTab::BOARDING_LOOT)
    {
        const WidgetLayout layout = buildLayout(this->widgetRect);
        if (!isPointInRect(mouse_x, mouse_y, layout.boardingLootLeftPanel) &&
            !isPointInRect(mouse_x, mouse_y, layout.boardingLootRightPanel))
        {
            return true;
        }

        const BoardingLootMetrics metrics = buildBoardingLootMetrics(
            layout.boardingLootLeftPanel,
            layout.boardingLootRightPanel,
            static_cast<int>(this->boardingLootCurrencies.size()));
        if (metrics.maxFirstRow > 0)
        {
            const int rowBefore = this->boardingLootFirstRow;
            this->boardingLootFirstRow -= delta;
            this->boardingLootFirstRow = (std::max)(0, (std::min)(this->boardingLootFirstRow, metrics.maxFirstRow));
            if (this->boardingLootFirstRow != rowBefore)
            {
                this->boardingLootScrollWheelHighlightSec = kScrollThumbWheelHighlightSec;
            }
        }
        return true;
    }

    if (this->activeTab == ActiveTab::ELITE_SHIPS || this->activeTab == ActiveTab::SPECIAL_SHIPS)
    {
        std::vector<InternalShipEntry>* ships = this->getShipsForTab(this->activeTab);
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
    drawTab(layout.tabStorageEquipped, "Depot / Equipe", self->activeTab == ActiveTab::STORAGE_EQUIPPED);
    drawTab(layout.tabBoardingLoot, "Gestion de butin d'abordage", self->activeTab == ActiveTab::BOARDING_LOOT);
    drawTab(layout.tabShipManagement, "Gestion du navire", self->activeTab == ActiveTab::SHIP_MANAGEMENT);
    drawTab(layout.tabAppearance, "Apparence", self->activeTab == ActiveTab::APPEARANCE);
    drawTab(layout.tabElite, "Navires elite acquis", self->activeTab == ActiveTab::ELITE_SHIPS);
    drawTab(layout.tabSpecial, "Navires speciaux acquis", self->activeTab == ActiveTab::SPECIAL_SHIPS);

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
            if (self->storageDropdownArrowImage.sdl_texture != nullptr)
            {
                static constexpr float kArrowSize = 18.0f;
                const SDL_FRect arrowRect = SDL_FRect{
                    r.x + r.w - kArrowSize - 8.0f,
                    r.y + (r.h - kArrowSize) * 0.5f,
                    kArrowSize,
                    kArrowSize
                };
                drawImageFit(&self->storageDropdownArrowImage, arrowRect);
            }
        };

        drawStoragePanel(layout.storageLeftPanel);
        drawStoragePanel(layout.storageMiddlePanel);

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

        const int equipmentIndex = self->storageEquipmentOptions.empty()
            ? -1
            : (std::max)(0, (std::min)(self->selectedStorageEquipmentOption, static_cast<int>(self->storageEquipmentOptions.size()) - 1));
        const std::string equipmentLabel = (equipmentIndex >= 0 && equipmentIndex < static_cast<int>(self->storageEquipmentOptions.size()))
            ? self->storageEquipmentOptions[static_cast<std::size_t>(equipmentIndex)].name
            : std::string("-");

        drawStorageDropdown(
            layout.storageLeftDropdown,
            equipmentLabel,
            self->openPicker == AppearancePickerType::STORAGE_ITEM_LEFT);
        drawStorageDropdown(
            layout.storageMiddleDropdown,
            equipmentLabel,
            self->openPicker == AppearancePickerType::STORAGE_ITEM_RIGHT);

        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);
        const bool leftDropReady =
            self->storageDrag.active &&
            self->storageDrag.source == StorageTabTransferLocation::SHIP &&
            isPointInRect(mouseX, mouseY, layout.storageLeftContent);
        const bool middleDropReady =
            self->storageDrag.active &&
            self->storageDrag.source == StorageTabTransferLocation::WAREHOUSE &&
            isPointInRect(mouseX, mouseY, layout.storageMiddleContent) &&
            self->getStorageRemainingEquipCapacityForCategory(self->storageDrag.category) > 0;
        const StorageTabItemEntry* hoveredCannonItem = nullptr;
        const RC2D_Image* hoveredCannonIcon = nullptr;

        auto drawStorageContent = [&](const SDL_FRect& r, bool dropReady) {
            rc2d_graphics_setColor(dropReady ? kStorageDropReadyFill : kPanelFill);
            rc2d_graphics_rectangle("fill", &r);
            rc2d_graphics_setColor(dropReady ? kStorageDropReadyLine : kGold);
            rc2d_graphics_rectangle("line", &r);
        };

        auto drawStorageRows = [&](StorageTabTransferLocation location, const SDL_FRect& contentRect, const char* emptyText) {
            const std::vector<StorageTabItemEntry>* items = self->getStorageItemsForLocation(location);
            const std::vector<RC2D_Image>* icons = self->getStorageItemIconsForLocation(location);
            if (items == nullptr)
            {
                return;
            }

            const int rowCapacity = getStorageVisibleRowCapacity(contentRect);
            int visibleRow = 0;
            int hiddenRows = 0;
            for (int sourceIndex = 0; sourceIndex < static_cast<int>(items->size()); ++sourceIndex)
            {
                const StorageTabItemEntry& item = (*items)[static_cast<std::size_t>(sourceIndex)];
                if (!self->isStorageItemVisibleForSelectedCategory(item) || item.quantity <= 0)
                {
                    continue;
                }

                if (visibleRow >= rowCapacity)
                {
                    ++hiddenRows;
                    continue;
                }

                const SDL_FRect rowRect = getStorageItemRowRect(contentRect, visibleRow);
                const bool draggingThis =
                    self->storageDrag.active &&
                    self->storageDrag.source == location &&
                    self->storageDrag.sourceIndex == sourceIndex;
                const bool disabledForEquip =
                    location == StorageTabTransferLocation::WAREHOUSE &&
                    self->isStorageWarehouseItemDisabledForEquip(item);

                rc2d_graphics_setColor(disabledForEquip ? RC2D_Color{10, 10, 12, 210} : (draggingThis ? kSelectionFill : kFieldFill));
                rc2d_graphics_rectangle("fill", &rowRect);
                rc2d_graphics_setColor(disabledForEquip ? RC2D_Color{72, 72, 78, 190} : kRowLine);
                rc2d_graphics_rectangle("line", &rowRect);

                const SDL_FRect itemIconRect = SDL_FRect{rowRect.x + 8.0f, rowRect.y + 8.0f, 56.0f, 56.0f};
                rc2d_graphics_setColor(disabledForEquip ? RC2D_Color{14, 14, 16, 228} : kPanelFill);
                rc2d_graphics_rectangle("fill", &itemIconRect);
                rc2d_graphics_setColor(disabledForEquip ? RC2D_Color{82, 82, 88, 210} : kGold);
                rc2d_graphics_rectangle("line", &itemIconRect);
                if (icons != nullptr && sourceIndex >= 0 && sourceIndex < static_cast<int>(icons->size()))
                {
                    drawImageFit(&(*icons)[static_cast<std::size_t>(sourceIndex)], itemIconRect);
                }
                if (disabledForEquip)
                {
                    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 145});
                    rc2d_graphics_rectangle("fill", &itemIconRect);
                }

                const bool canShowCannonOverlay =
                    item.category == StorageTabEquipmentCategory::CANNONS &&
                    isPointInRect(mouseX, mouseY, rowRect);
                if (!self->storageDrag.active && canShowCannonOverlay)
                {
                    hoveredCannonItem = &item;
                    hoveredCannonIcon = (icons != nullptr &&
                        sourceIndex >= 0 &&
                        sourceIndex < static_cast<int>(icons->size()))
                        ? &(*icons)[static_cast<std::size_t>(sourceIndex)]
                        : nullptr;
                }

                const float textX = itemIconRect.x + itemIconRect.w + 10.0f;
                drawTextAt(
                    &self->bodyFont,
                    item.name.empty() ? std::string("-") : item.name,
                    textX,
                    rowRect.y + 13.0f,
                    disabledForEquip ? kTextMuted : kTextGold);
                drawTextAt(
                    &self->smallFont,
                    disabledForEquip ? std::string("Maximum equipe") : ("Quantite : " + std::to_string(item.quantity)),
                    textX,
                    rowRect.y + 42.0f,
                    disabledForEquip ? kTextMuted : kTextBody);

                ++visibleRow;
            }

            if (visibleRow == 0)
            {
                drawCentered(&self->bodyFont, emptyText, contentRect, kTextMuted);
            }
            else if (hiddenRows > 0)
            {
                const std::string more = "+" + std::to_string(hiddenRows) + " autre(s)";
                drawTextAt(
                    &self->smallFont,
                    more,
                    contentRect.x + 10.0f,
                    contentRect.y + contentRect.h - 22.0f,
                    kTextMuted);
            }
        };

        drawStorageContent(layout.storageLeftContent, leftDropReady);
        drawStorageContent(layout.storageMiddleContent, middleDropReady);
        drawStorageRows(StorageTabTransferLocation::WAREHOUSE, layout.storageLeftContent, "Aucun objet en entrepot");
        drawStorageRows(StorageTabTransferLocation::SHIP, layout.storageMiddleContent, "Aucun objet equipe");

        auto drawHoveredCannonTooltip = [&]() {
            if (hoveredCannonItem != nullptr)
            {
                const float tooltipW = 336.0f;
                const float tooltipH = 184.0f;
                float tooltipX = mouseX + 18.0f;
                float tooltipY = mouseY + 16.0f;
                if (tooltipX + tooltipW > self->widgetRect.x + self->widgetRect.w - 8.0f)
                {
                    tooltipX = mouseX - tooltipW - 18.0f;
                }
                if (tooltipY + tooltipH > self->widgetRect.y + self->widgetRect.h - 8.0f)
                {
                    tooltipY = mouseY - tooltipH - 16.0f;
                }
                tooltipX = clampf(tooltipX, self->widgetRect.x + 8.0f, self->widgetRect.x + self->widgetRect.w - tooltipW - 8.0f);
                tooltipY = clampf(tooltipY, self->widgetRect.y + 8.0f, self->widgetRect.y + self->widgetRect.h - tooltipH - 8.0f);

                const SDL_FRect tooltipRect = SDL_FRect{tooltipX, tooltipY, tooltipW, tooltipH};
                const SDL_FRect tooltipHeader = SDL_FRect{tooltipRect.x, tooltipRect.y, tooltipRect.w, 36.0f};
                rc2d_graphics_setColor(kPanelFill);
                rc2d_graphics_rectangle("fill", &tooltipRect);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("line", &tooltipRect);
                rc2d_graphics_setColor(kHeaderFill);
                rc2d_graphics_rectangle("fill", &tooltipHeader);
                rc2d_graphics_setColor(kGold);
                rc2d_graphics_rectangle("line", &tooltipHeader);

                drawTextAt(
                    &self->bodyFont,
                    hoveredCannonItem->name.empty() ? std::string("Canon") : hoveredCannonItem->name,
                    tooltipHeader.x + 10.0f,
                    tooltipHeader.y + 8.0f,
                    kTextGold);

                auto valOrDash = [](const std::string& s) -> std::string {
                    return s.empty() ? std::string("-") : s;
                };
                const std::array<std::pair<std::string, std::string>, 5> cannonRows = {{
                    {"Degats des cannons", valOrDash(hoveredCannonItem->cannonStats.damageDisplay)},
                    {"Degats critique des cannons", valOrDash(hoveredCannonItem->cannonStats.critDamageDisplay)},
                    {"Chance de coup critique des cannons", valOrDash(hoveredCannonItem->cannonStats.critChanceDisplay)},
                    {"Portee des cannons", valOrDash(hoveredCannonItem->cannonStats.rangeDisplay)},
                    {"Temps de recharge des cannons", valOrDash(hoveredCannonItem->cannonStats.reloadDisplay)}
                }};

                const float rowTop = tooltipHeader.y + tooltipHeader.h + 6.0f;
                const float rowH = 27.0f;
                for (int row = 0; row < static_cast<int>(cannonRows.size()); ++row)
                {
                    const SDL_FRect rowRect = SDL_FRect{
                        tooltipRect.x + 7.0f,
                        rowTop + (static_cast<float>(row) * rowH),
                        tooltipRect.w - 14.0f,
                        rowH
                    };
                    rc2d_graphics_setColor(kFieldFill);
                    rc2d_graphics_rectangle("fill", &rowRect);
                    rc2d_graphics_setColor(kRowLine);
                    rc2d_graphics_rectangle("line", &rowRect);

                    const std::string& label = cannonRows[static_cast<std::size_t>(row)].first;
                    const std::string& value = cannonRows[static_cast<std::size_t>(row)].second;
                    drawTextAt(&self->smallFont, label, rowRect.x + 7.0f, rowRect.y + 6.0f, kTextGold);
                    const float valueW = measureTextWidth(&self->smallFont, value);
                    drawTextAt(&self->smallFont, value, rowRect.x + rowRect.w - valueW - 7.0f, rowRect.y + 6.0f, kTextBody);
                }
            }
        };

        drawHoveredCannonTooltip();
    }
    else if (self->activeTab == ActiveTab::BOARDING_LOOT)
    {
        auto drawLootPanel = [&](const SDL_FRect& panel, const char* title) {
            rc2d_graphics_setColor(kPanelFill);
            rc2d_graphics_rectangle("fill", &panel);
            rc2d_graphics_setColor(kGold);
            rc2d_graphics_rectangle("line", &panel);
            drawCentered(
                &self->smallFont,
                title,
                SDL_FRect{panel.x, panel.y - 18.0f, panel.w, 18.0f},
                kTextGold);
        };

        const bool canTransferAny = std::any_of(
            self->boardingLootCurrencies.begin(),
            self->boardingLootCurrencies.end(),
            [](const BoardingLootCurrencyEntry& currency) {
                const int quantity = (std::max)(0, currency.shipQuantity);
                const int minQuantity = (std::max)(0, currency.minQuantityToSecureReserve);
                return !currency.forceStoreOnShip && quantity > 0 && quantity >= minQuantity;
            });

        rc2d_graphics_setColor(canTransferAny ? kButtonFill : kFieldFill);
        rc2d_graphics_rectangle("fill", &layout.boardingLootTransferAllButton);
        rc2d_graphics_setColor(canTransferAny ? kStorageDropReadyLine : kRowLine);
        rc2d_graphics_rectangle("line", &layout.boardingLootTransferAllButton);
        drawCentered(
            &self->smallFont,
            "Tout transferer vers la reserve securisee",
            layout.boardingLootTransferAllButton,
            canTransferAny ? kTextGold : kTextMuted);

        drawLootPanel(layout.boardingLootLeftPanel, "Sur le navire");
        drawLootPanel(layout.boardingLootRightPanel, "Reserve securisee");

        if (self->boardingLootCurrencies.empty())
        {
            drawCentered(&self->bodyFont, "Aucun butin d'abordage", layout.boardingLootLeftPanel, kTextMuted);
            drawCentered(&self->bodyFont, "Aucune reserve", layout.boardingLootRightPanel, kTextMuted);
        }
        else
        {
            const BoardingLootMetrics metrics = buildBoardingLootMetrics(
                layout.boardingLootLeftPanel,
                layout.boardingLootRightPanel,
                static_cast<int>(self->boardingLootCurrencies.size()));
            int firstRow = (std::max)(0, (std::min)(self->boardingLootFirstRow, metrics.maxFirstRow));
            const int lastRow = (std::min)(metrics.totalRows, firstRow + metrics.visibleRows);

            auto drawCurrencyRow = [&](const SDL_FRect& panel,
                                       int visualRow,
                                       const RC2D_Image* icon,
                                       const std::string& value,
                                       const char* note,
                                       bool muted) {
                const SDL_FRect rowRect = getBoardingLootRowRect(panel, visualRow);
                rc2d_graphics_setColor(muted ? RC2D_Color{12, 12, 14, 210} : kFieldFill);
                rc2d_graphics_rectangle("fill", &rowRect);
                rc2d_graphics_setColor(muted ? RC2D_Color{72, 72, 78, 190} : kRowLine);
                rc2d_graphics_rectangle("line", &rowRect);

                const SDL_FRect iconRect = SDL_FRect{
                    rowRect.x + 8.0f,
                    rowRect.y + ((rowRect.h - kBoardingLootIconSize) * 0.5f),
                    kBoardingLootIconSize,
                    kBoardingLootIconSize
                };
                rc2d_graphics_setColor(muted ? RC2D_Color{18, 18, 20, 220} : kPanelFill);
                rc2d_graphics_rectangle("fill", &iconRect);
                rc2d_graphics_setColor(muted ? RC2D_Color{72, 72, 78, 190} : kGold);
                rc2d_graphics_rectangle("line", &iconRect);
                drawImageFit(icon, iconRect);

                const float valueW = measureTextWidth(&self->bodyFont, value);
                drawTextAt(
                    &self->bodyFont,
                    value,
                    rowRect.x + rowRect.w - valueW - 8.0f,
                    rowRect.y + 9.0f,
                    muted ? kTextMuted : kTextBody);

                if (note != nullptr && note[0] != '\0')
                {
                    drawTextAt(
                        &self->smallFont,
                        note,
                        iconRect.x + iconRect.w + 10.0f,
                        rowRect.y + 32.0f,
                        kTextMuted);
                }
            };

            for (int sourceRow = firstRow; sourceRow < lastRow; ++sourceRow)
            {
                const BoardingLootCurrencyEntry& currency =
                    self->boardingLootCurrencies[static_cast<std::size_t>(sourceRow)];
                const int visualRow = sourceRow - firstRow;
                const bool lockedToShip = currency.forceStoreOnShip;
                const int minQuantity = (std::max)(0, currency.minQuantityToSecureReserve);
                std::string shipNote;
                if (lockedToShip)
                {
                    shipNote = "Stockage obligatoire sur le navire";
                }
                else if (minQuantity > 0 && currency.shipQuantity < minQuantity)
                {
                    shipNote = "Minimum " + formatWithDots(minQuantity) + " pour transferer dans la reserve securisee";
                }
                const RC2D_Image* icon =
                    sourceRow >= 0 && sourceRow < static_cast<int>(self->boardingLootCurrencyIcons.size())
                        ? &self->boardingLootCurrencyIcons[static_cast<std::size_t>(sourceRow)]
                        : nullptr;

                drawCurrencyRow(
                    metrics.leftBody,
                    visualRow,
                    icon,
                    formatWithDots(currency.shipQuantity),
                    shipNote.c_str(),
                    false);
                drawCurrencyRow(
                    metrics.rightBody,
                    visualRow,
                    icon,
                    lockedToShip ? std::string("-") : formatWithDots(currency.secureReserveQuantity),
                    lockedToShip ? "Bloque sur le navire" : "",
                    lockedToShip);
            }

            if (metrics.maxFirstRow > 0)
            {
                rc2d_graphics_setColor(kScrollTrack);
                rc2d_graphics_rectangle("fill", &metrics.scrollTrack);

                const float thumbHeight = (std::max)(
                    kMinThumbHeight,
                    (metrics.scrollTrack.h * static_cast<float>(metrics.visibleRows) /
                        static_cast<float>((std::max)(1, metrics.totalRows))));
                const float thumbTravel = (std::max)(1.0f, metrics.scrollTrack.h - thumbHeight);
                const float ratio = static_cast<float>(firstRow) / static_cast<float>(metrics.maxFirstRow);
                const SDL_FRect thumb = SDL_FRect{
                    metrics.scrollTrack.x,
                    metrics.scrollTrack.y + (thumbTravel * ratio),
                    metrics.scrollTrack.w,
                    thumbHeight
                };
                rc2d_graphics_setColor(
                    self->boardingLootScrollDragging || (self->boardingLootScrollWheelHighlightSec > 0.0f)
                        ? kScrollThumbDragFill
                        : kScrollThumb);
                rc2d_graphics_rectangle("fill", &thumb);
            }
        }
    }
    else
    {
        const bool eliteTab = self->activeTab == ActiveTab::ELITE_SHIPS;
        std::vector<InternalShipEntry>* ships = self->getShipsForTab(self->activeTab);
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

                const AccountTabEliteProgressData eliteProgress = self->getAccountTabEliteProgress();
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

                const InternalShipEntry& entry = (*ships)[static_cast<std::size_t>(sourceIndex)];

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

    if (self->storageTransferPopup.open)
    {
        const StorageTransferPopupLayout popupLayout = buildStorageTransferPopupLayout(self->widgetRect);
        const std::vector<RC2D_Image>* sourceIcons =
            self->getStorageItemIconsForLocation(self->storageTransferPopup.source);
        RC2D_Image* popupIcon = nullptr;
        if (sourceIcons != nullptr &&
            self->storageTransferPopup.sourceIndex >= 0 &&
            self->storageTransferPopup.sourceIndex < static_cast<int>(sourceIcons->size()))
        {
            popupIcon = const_cast<RC2D_Image*>(&(*sourceIcons)[static_cast<std::size_t>(self->storageTransferPopup.sourceIndex)]);
        }

        rc2d_graphics_setColor(kPopupOverlayFill);
        rc2d_graphics_rectangle("fill", &popupLayout.overlay);
        rc2d_graphics_setColor(kPanelFill);
        rc2d_graphics_rectangle("fill", &popupLayout.panel);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &popupLayout.panel);

        rc2d_graphics_setColor(kHeaderFill);
        rc2d_graphics_rectangle("fill", &popupLayout.titleBar);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &popupLayout.titleBar);
        drawCentered(&self->bodyFont, "Transfert", popupLayout.titleBar, kTextGold);

        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &popupLayout.icon);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &popupLayout.icon);
        drawImageFit(popupIcon, popupLayout.icon);

        const std::string directionLabel =
            std::string(storageLocationLabel(self->storageTransferPopup.source)) +
            " -> " +
            storageLocationLabel(self->storageTransferPopup.target);
        drawTextAt(
            &self->bodyFont,
            self->storageTransferPopup.itemName.empty() ? std::string("-") : self->storageTransferPopup.itemName,
            popupLayout.icon.x + popupLayout.icon.w + 24.0f,
            popupLayout.icon.y - 2.0f,
            kTextGold);
        drawTextAt(
            &self->smallFont,
            directionLabel,
            popupLayout.icon.x + popupLayout.icon.w + 24.0f,
            popupLayout.icon.y + 24.0f,
            kTextBody);
        drawTextAt(
            &self->smallFont,
            "Quantite a transferer",
            popupLayout.icon.x + popupLayout.icon.w + 24.0f,
            popupLayout.icon.y + 50.0f,
            kTextMuted);

        auto drawPopupButton = [&](const SDL_FRect& r, const char* label, bool primary) {
            rc2d_graphics_setColor(primary ? kButtonFill : kFieldFill);
            rc2d_graphics_rectangle("fill", &r);
            rc2d_graphics_setColor(primary ? kStorageDropReadyLine : kGold);
            rc2d_graphics_rectangle("line", &r);
            drawCentered(&self->smallFont, label, r, primary ? kTextGold : kTextBody);
        };

        drawPopupButton(popupLayout.quantityMinusButton, "-", false);
        rc2d_graphics_setColor(kFieldFill);
        rc2d_graphics_rectangle("fill", &popupLayout.quantityValue);
        rc2d_graphics_setColor(kGold);
        rc2d_graphics_rectangle("line", &popupLayout.quantityValue);
        const std::string quantityLabel =
            std::to_string(self->storageTransferPopup.quantity) +
            " / " +
            std::to_string((std::max)(1, self->storageTransferPopup.maxQuantity));
        drawCentered(&self->bodyFont, quantityLabel.c_str(), popupLayout.quantityValue, kTextGold);
        drawPopupButton(popupLayout.quantityPlusButton, "+", false);
        drawPopupButton(popupLayout.cancelButton, "ANNULER", false);
        drawPopupButton(popupLayout.transferButton, "TRANSFERT", true);
    }

    if (self->storageDrag.active)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        getMouseRenderPosition(&mouseX, &mouseY);

        const std::vector<RC2D_Image>* dragIcons = self->getStorageItemIconsForLocation(self->storageDrag.source);
        RC2D_Image* dragIcon = nullptr;
        if (dragIcons != nullptr &&
            self->storageDrag.sourceIndex >= 0 &&
            self->storageDrag.sourceIndex < static_cast<int>(dragIcons->size()))
        {
            dragIcon = const_cast<RC2D_Image*>(&(*dragIcons)[static_cast<std::size_t>(self->storageDrag.sourceIndex)]);
        }

        const SDL_FRect ghostRect = SDL_FRect{
            mouseX - (kStorageDragIconSize * 0.5f),
            mouseY - (kStorageDragIconSize * 0.5f),
            kStorageDragIconSize,
            kStorageDragIconSize
        };
        rc2d_graphics_setColor(kStorageDragGhostFill);
        rc2d_graphics_rectangle("fill", &ghostRect);
        rc2d_graphics_setColor(kStorageDropReadyLine);
        rc2d_graphics_rectangle("line", &ghostRect);
        drawImageFit(dragIcon, ghostRect);
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}
