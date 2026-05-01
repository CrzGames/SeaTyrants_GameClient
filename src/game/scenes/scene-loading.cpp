#include "game/scenes/scene-loading.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/context.h"
#include "game/assets/title-asset-cache.h"

static std::string loadingScene_getFriendlyStageLabel(const TitleAssetPreloadProgress& progress)
{
    if (progress.finished)
    {
        return "Derniers preparatifs avant le depart...";
    }

    switch (progress.currentKind)
    {
        case TitleAssetKind::IMAGE:
            return "Preparation des visuels du jeu...";
        case TitleAssetKind::FONT:
            return "Preparation des textes et des menus...";
        case TitleAssetKind::AUDIO:
            return "Preparation des sons et des musiques...";
        case TitleAssetKind::VIDEO:
            return "Preparation des videos...";
        default:
            return "Preparation de votre aventure...";
    }
}

static std::string loadingScene_buildProgressLabel(const TitleAssetPreloadProgress& progress)
{
    return std::to_string(progress.processedEntries) +
        " / " +
        std::to_string(progress.totalEntries) +
        " ressources chargees";
}

static std::string loadingScene_buildCountersLabel(const TitleAssetPreloadProgress& progress)
{
    return "Images : " + std::to_string(progress.loadedImageCount) +
        "  |  Fonts : " + std::to_string(progress.loadedFontCount) +
        "  |  Videos : " + std::to_string(progress.warmedVideoCount) +
        "  |  Sounds : " + std::to_string(progress.loadedAudioCount);
}

static std::string loadingScene_buildFooterLabel(const TitleAssetPreloadProgress& progress)
{
    (void)progress;
    return "";
}

static void loadingScene_drawTextLine(const RC2D_Font* font, const std::string& text, float x, float y, RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text renderedText = rc2d_graphics_createText(const_cast<RC2D_Font*>(font), text.c_str());
    renderedText.color = color;
    rc2d_graphics_setTextColor(&renderedText);
    rc2d_graphics_drawText(&renderedText, x, y);
    rc2d_graphics_destroyText(&renderedText);
}

static void loadingScene_drawTextCenteredInRect(
    const RC2D_Font* font,
    const std::string& text,
    const SDL_FRect& rect,
    RC2D_Color color)
{
    if (font == nullptr || font->sdl_font == nullptr || text.empty())
    {
        return;
    }

    RC2D_Text renderedText = rc2d_graphics_createText(const_cast<RC2D_Font*>(font), text.c_str());
    renderedText.color = color;
    rc2d_graphics_setTextColor(&renderedText);

    int textWidth = 0;
    int textHeight = 0;
    rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);

    const float drawX = rect.x + ((rect.w - static_cast<float>(textWidth)) * 0.5f);
    const float drawY = rect.y + ((rect.h - static_cast<float>(textHeight)) * 0.5f) - 1.0f;
    rc2d_graphics_drawText(&renderedText, drawX, drawY);
    rc2d_graphics_destroyText(&renderedText);
}

static void loadingScene_drawBackgroundCover(const RC2D_Image* backgroundImage, const SDL_FRect& screenRect)
{
    if (backgroundImage == nullptr || backgroundImage->sdl_texture == nullptr)
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        rc2d_graphics_setColor(RC2D_Color{9, 18, 28, 255});
        rc2d_graphics_rectangle("fill", &screenRect);
        return;
    }

    float textureWidth = 0.0f;
    float textureHeight = 0.0f;
    if (!SDL_GetTextureSize(backgroundImage->sdl_texture, &textureWidth, &textureHeight) ||
        textureWidth <= 0.0f ||
        textureHeight <= 0.0f)
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        rc2d_graphics_setColor(RC2D_Color{9, 18, 28, 255});
        rc2d_graphics_rectangle("fill", &screenRect);
        return;
    }

    const float scaleX = screenRect.w / textureWidth;
    const float scaleY = screenRect.h / textureHeight;
    const float scale = (std::max)(scaleX, scaleY);
    const float drawWidth = textureWidth * scale;
    const float drawHeight = textureHeight * scale;
    const float drawX = screenRect.x + ((screenRect.w - drawWidth) * 0.5f);
    const float drawY = screenRect.y + ((screenRect.h - drawHeight) * 0.5f);

    rc2d_graphics_drawImage(
        const_cast<RC2D_Image*>(backgroundImage),
        drawX,
        drawY,
        0.0,
        scale,
        scale,
        0.0f,
        0.0f,
        false,
        false);
}

double LoadingScene::clamp01(double value)
{
    if (value < 0.0)
    {
        return 0.0;
    }

    if (value > 1.0)
    {
        return 1.0;
    }

    return value;
}

void LoadingScene::drawFullscreenBlackWithAlpha(double alpha01)
{
    const double alphaClamped = clamp01(alpha01);
    if (alphaClamped <= 0.0)
    {
        return;
    }

    SDL_FRect rect = GetGameScreen().rect;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor({0, 0, 0, static_cast<Uint8>(alphaClamped * 255.0)});
    rc2d_graphics_rectangle("fill", &rect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

std::string LoadingScene::getDefaultNextSceneName()
{
#if GAME_ENV_DEV
    return "editormap-shipdownscale";
#else
    return "splashscreen";
#endif
}

LoadingScene::LoadingScene()
    : nextSceneName(getDefaultNextSceneName()),
      backgroundImage{},
      headingFont{},
      bodyFont{},
      transitionStarted(false),
      transitionFadeOutStarted(false),
      transitionDelayRemaining(0.18),
      loadingFadeAlpha(1.0f)
{
}

LoadingScene::LoadingScene(const char* nextSceneNameValue)
    : LoadingScene((nextSceneNameValue != nullptr) ? std::string(nextSceneNameValue) : std::string())
{
}

LoadingScene::LoadingScene(const std::string& nextSceneNameValue)
    : nextSceneName(nextSceneNameValue.empty() ? getDefaultNextSceneName() : nextSceneNameValue),
      backgroundImage{},
      headingFont{},
      bodyFont{},
      transitionStarted(false),
      transitionFadeOutStarted(false),
      transitionDelayRemaining(0.18),
      loadingFadeAlpha(1.0f)
{
}

LoadingScene::~LoadingScene()
{
}

void LoadingScene::load(void)
{
    this->transitionStarted = false;
    this->transitionFadeOutStarted = false;
    this->transitionDelayRemaining = 0.18;
    this->loadingFadeAlpha = 1.0f;
    rc2d_mouse_setVisible(false);

    this->backgroundImage = LoadStorageImage(
        "assets/images/ui-scene-loading/background.png",
        RC2D_STORAGE_TITLE);
    this->headingFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 24.0f);
    this->bodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 18.0f);

    GetTitleAssetCache().beginFullPreload();
}

void LoadingScene::unload(void)
{
    this->transitionStarted = false;
    this->transitionFadeOutStarted = false;
    this->transitionDelayRemaining = 0.18;
    this->loadingFadeAlpha = 1.0f;
    ResetStorageImageRef(&this->backgroundImage);
    GetTitleAssetCache().evictImage("assets/images/ui-scene-loading/background.png", RC2D_STORAGE_TITLE);
    ResetStorageFontRef(&this->headingFont);
    ResetStorageFontRef(&this->bodyFont);
}

void LoadingScene::update(double dt)
{
    const double safeDt = (std::max)(dt, 0.0);
    const float fadeStep = static_cast<float>(kLoadingFadeSpeed * safeDt);

    if (!this->transitionFadeOutStarted && this->loadingFadeAlpha > 0.0f)
    {
        this->loadingFadeAlpha -= fadeStep;
        if (this->loadingFadeAlpha < 0.0f)
        {
            this->loadingFadeAlpha = 0.0f;
        }
    }

    TitleAssetCache& assetCache = GetTitleAssetCache();
    if (!assetCache.isPreloadFinished())
    {
        assetCache.preloadNextBatch(7.5, 36);
        return;
    }

    if (this->transitionDelayRemaining > 0.0)
    {
        this->transitionDelayRemaining = (std::max)(0.0, this->transitionDelayRemaining - safeDt);
        return;
    }

    this->transitionFadeOutStarted = true;

    if (this->loadingFadeAlpha < 1.0f)
    {
        this->loadingFadeAlpha += fadeStep;
        if (this->loadingFadeAlpha > 1.0f)
        {
            this->loadingFadeAlpha = 1.0f;
        }
    }

    if (!this->transitionStarted && this->sceneManager != nullptr && this->loadingFadeAlpha >= 1.0f)
    {
        this->transitionStarted = true;
        rc2d_mouse_setVisible(true);
        this->sceneManager->changeScene(this->nextSceneName);
    }
}

void LoadingScene::draw(void)
{
    const SDL_FRect screenRect = GetGameScreen().rect;
    const float width = (screenRect.w > 0.0f) ? screenRect.w : 1920.0f;
    const float height = (screenRect.h > 0.0f) ? screenRect.h : 1080.0f;
    const float originX = screenRect.x;
    const float originY = screenRect.y;
    const SDL_FRect fullRect{originX, originY, width, height};

    loadingScene_drawBackgroundCover(&this->backgroundImage, fullRect);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(RC2D_Color{4, 10, 18, 116});
    rc2d_graphics_rectangle("fill", &fullRect);

    const TitleAssetPreloadProgress& progress = GetTitleAssetCache().getPreloadProgress();
    const float ratio =
        (progress.totalEntries > 0)
            ? static_cast<float>(progress.processedEntries) / static_cast<float>(progress.totalEntries)
            : 1.0f;
    const float clampedRatio = std::clamp(ratio, 0.0f, 1.0f);
    const int percent = static_cast<int>(std::lround(static_cast<double>(clampedRatio) * 100.0));

    const float panelWidth = (std::min)(width * 0.74f, 960.0f);
    const float panelHeight = 236.0f;
    const float panelX = originX + ((width - panelWidth) * 0.5f);
    const float panelY = originY + ((height - panelHeight) * 0.5f);
    const SDL_FRect panelRect{panelX, panelY, panelWidth, panelHeight};

    const SDL_FRect shadowRect{panelRect.x + 10.0f, panelRect.y + 12.0f, panelRect.w, panelRect.h};
    rc2d_graphics_setColor(RC2D_Color{0, 0, 0, 96});
    rc2d_graphics_rectangle("fill", &shadowRect);

    rc2d_graphics_setColor(RC2D_Color{5, 10, 19, 228});
    rc2d_graphics_rectangle("fill", &panelRect);
    rc2d_graphics_setColor(RC2D_Color{209, 158, 45, 255});
    rc2d_graphics_rectangle("line", &panelRect);

    const SDL_FRect innerRect{panelRect.x + 4.0f, panelRect.y + 4.0f, panelRect.w - 8.0f, panelRect.h - 8.0f};
    rc2d_graphics_setColor(RC2D_Color{163, 118, 28, 255});
    rc2d_graphics_rectangle("line", &innerRect);

    const SDL_FRect headerRect{panelRect.x + 4.0f, panelRect.y + 4.0f, panelRect.w - 8.0f, 38.0f};
    rc2d_graphics_setColor(RC2D_Color{81, 18, 14, 236});
    rc2d_graphics_rectangle("fill", &headerRect);
    rc2d_graphics_setColor(RC2D_Color{209, 158, 45, 255});
    rc2d_graphics_rectangle("line", &headerRect);

    const SDL_FRect percentBadge{headerRect.x + headerRect.w - 106.0f, headerRect.y + 4.0f, 90.0f, 30.0f};
    rc2d_graphics_setColor(RC2D_Color{12, 35, 52, 255});
    rc2d_graphics_rectangle("fill", &percentBadge);
    rc2d_graphics_setColor(RC2D_Color{209, 158, 45, 255});
    rc2d_graphics_rectangle("line", &percentBadge);

    const SDL_FRect barOuter{panelRect.x + 34.0f, panelRect.y + 124.0f, panelRect.w - 68.0f, 24.0f};
    rc2d_graphics_setColor(RC2D_Color{10, 18, 31, 244});
    rc2d_graphics_rectangle("fill", &barOuter);
    rc2d_graphics_setColor(RC2D_Color{209, 158, 45, 255});
    rc2d_graphics_rectangle("line", &barOuter);

    const SDL_FRect barInner{barOuter.x + 2.0f, barOuter.y + 2.0f, barOuter.w - 4.0f, barOuter.h - 4.0f};
    rc2d_graphics_setColor(RC2D_Color{13, 28, 44, 255});
    rc2d_graphics_rectangle("fill", &barInner);

    const SDL_FRect barFill{
        barInner.x,
        barInner.y,
        std::clamp(barInner.w * clampedRatio, 0.0f, barInner.w),
        barInner.h};
    rc2d_graphics_setColor(RC2D_Color{19, 64, 92, 255});
    rc2d_graphics_rectangle("fill", &barFill);

    const SDL_FRect barHighlight{
        barFill.x,
        barFill.y,
        barFill.w,
        (std::max)(5.0f, barFill.h * 0.42f)};
    rc2d_graphics_setColor(RC2D_Color{73, 127, 158, 92});
    rc2d_graphics_rectangle("fill", &barHighlight);

    if (barFill.w >= 10.0f)
    {
        const SDL_FRect leadingCap{
            barFill.x + barFill.w - 6.0f,
            barFill.y,
            6.0f,
            barFill.h};
        rc2d_graphics_setColor(RC2D_Color{220, 182, 82, 255});
        rc2d_graphics_rectangle("fill", &leadingCap);
    }

    for (int markerIndex = 1; markerIndex <= 3; ++markerIndex)
    {
        const float markerX = barInner.x + ((barInner.w * static_cast<float>(markerIndex)) / 4.0f);
        SDL_FRect markerRect{markerX, barInner.y + 2.0f, 1.0f, barInner.h - 4.0f};
        rc2d_graphics_setColor(RC2D_Color{128, 97, 28, 130});
        rc2d_graphics_rectangle("fill", &markerRect);
    }

    if (this->headingFont.sdl_font != nullptr || this->bodyFont.sdl_font != nullptr)
    {
        const std::string title = "CHARGEMENT DES ASSETS";
        const std::string percentLabel = std::to_string(percent) + "%";
        const std::string subtitle = "Preparation de votre aventure en mer.";
        const std::string stateLabel = "En cours : " + loadingScene_getFriendlyStageLabel(progress);
        const std::string progressLabel = loadingScene_buildProgressLabel(progress);
        const std::string countersLabel = loadingScene_buildCountersLabel(progress);
        const std::string footerLabel = loadingScene_buildFooterLabel(progress);

        loadingScene_drawTextLine(
            &this->headingFont,
            title,
            headerRect.x + 18.0f,
            headerRect.y + 6.0f,
            RC2D_Color{235, 196, 97, 255});
        loadingScene_drawTextCenteredInRect(
            &this->headingFont,
            percentLabel,
            percentBadge,
            RC2D_Color{235, 196, 97, 255});
        loadingScene_drawTextLine(
            &this->bodyFont,
            subtitle,
            panelRect.x + 34.0f,
            panelRect.y + 60.0f,
            RC2D_Color{234, 232, 224, 255});
        loadingScene_drawTextLine(
            &this->bodyFont,
            stateLabel,
            panelRect.x + 34.0f,
            panelRect.y + 90.0f,
            RC2D_Color{229, 191, 96, 255});
        loadingScene_drawTextLine(
            &this->bodyFont,
            progressLabel,
            barOuter.x,
            barOuter.y + 32.0f,
            RC2D_Color{228, 225, 214, 248});
        loadingScene_drawTextLine(
            &this->bodyFont,
            countersLabel,
            panelRect.x + 34.0f,
            panelRect.y + 184.0f,
            RC2D_Color{229, 191, 96, 246});
        loadingScene_drawTextLine(
            &this->bodyFont,
            footerLabel,
            panelRect.x + 34.0f,
            panelRect.y + 208.0f,
            RC2D_Color{189, 194, 196, 244});
    }

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    if (this->loadingFadeAlpha > 0.0f)
    {
        this->drawFullscreenBlackWithAlpha(this->loadingFadeAlpha);
    }
}

void LoadingScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)scancode;
    (void)keycode;
    (void)mod;
    (void)isrepeat;
    (void)keyboardID;
}

void LoadingScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)x;
    (void)y;
    (void)button;
    (void)clicks;
    (void)mouseID;
}
