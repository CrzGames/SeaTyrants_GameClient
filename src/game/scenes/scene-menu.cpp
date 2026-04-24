#include "game/scenes/scene-menu.h"

#include "game/assets/title-asset-cache.h"

#include "core/context.h"
#include "game/scenes/scene-manager.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace menu_scene
{
    static constexpr std::array<const char*, 24> kLanguageFlagNames = {
        "albania",
        "austria",
        "belgium",
        "croatia",
        "czech_republic",
        "denmark",
        "england",
        "france",
        "georgia",
        "germany",
        "hungary",
        "italy",
        "netherlands",
        "poland",
        "portugal",
        "romania",
        "scotland",
        "serbia",
        "slovakia",
        "slovenia",
        "spain",
        "switzerland",
        "turkey",
        "ukraine"};

    static constexpr float kLanguageButtonWidth = 72.0f;
    static constexpr float kLanguageButtonHeight = 48.0f;
    static constexpr float kLanguageButtonMargin = 22.0f;
    static constexpr float kLanguageDropdownGap = 0.0f;
    static constexpr float kLanguageListPadding = 0.0f;
    static constexpr float kLanguageListItemHeight = kLanguageButtonHeight;
    static constexpr float kLanguageListGapY = 0.0f;
    static constexpr float kLanguageScrollBarWidth = 4.0f;
    static constexpr float kLanguageScrollBarPadding = 0.0f;
    static constexpr int kLanguageVisibleItemCount = 5;
    static constexpr float kLanguageScrollWheelStep = kLanguageListItemHeight + kLanguageListGapY;
    static constexpr float kLoginButtonPulseSpeed = 1.7f;
    static constexpr float kLoginButtonPulseScaleAmplitude = 0.022f;
    static constexpr float kMinScrollThumbHeight = 28.0f;

    static constexpr RC2D_Color kCardShadow = RC2D_Color{0, 0, 0, 124};
    static constexpr RC2D_Color kCardFill = RC2D_Color{7, 14, 22, 208};
    static constexpr RC2D_Color kCardInnerFill = RC2D_Color{12, 24, 38, 156};
    static constexpr RC2D_Color kCardBorder = RC2D_Color{204, 160, 63, 246};
    static constexpr RC2D_Color kCardInnerBorder = RC2D_Color{120, 165, 198, 170};
    static constexpr RC2D_Color kHeaderFill = RC2D_Color{20, 44, 66, 205};
    static constexpr RC2D_Color kHeaderLine = RC2D_Color{230, 193, 88, 236};
    static constexpr RC2D_Color kTitleText = RC2D_Color{240, 217, 148, 255};
    static constexpr RC2D_Color kBodyText = RC2D_Color{202, 214, 226, 240};
    static constexpr RC2D_Color kMutedText = RC2D_Color{137, 170, 194, 228};
    static constexpr RC2D_Color kFlagNormalFill = RC2D_Color{13, 24, 38, 198};
    static constexpr RC2D_Color kFlagHoverFill = RC2D_Color{28, 52, 77, 226};
    static constexpr RC2D_Color kFlagSelectedFill = RC2D_Color{40, 70, 96, 238};
    static constexpr RC2D_Color kFlagNormalBorder = RC2D_Color{105, 138, 164, 178};
    static constexpr RC2D_Color kFlagHoverBorder = RC2D_Color{218, 187, 96, 240};
    static constexpr RC2D_Color kFlagSelectedBorder = RC2D_Color{237, 210, 124, 255};

    /**
     * @brief Applique un alpha global a une couleur RGBA existante.
     */
    RC2D_Color applyAlpha(RC2D_Color color, float alpha01)
    {
        const float clamped = std::clamp(alpha01, 0.0f, 1.0f);
        color.a = static_cast<Uint8>(std::round(static_cast<float>(color.a) * clamped));
        return color;
    }

    /**
     * @brief Interpole lineairement entre deux valeurs.
     */
    double lerp(double startValue, double endValue, double ratio)
    {
        return startValue + ((endValue - startValue) * ratio);
    }

    /**
     * @brief Indique si un rectangle logique est exploitable.
     */
    bool isValidRect(const SDL_FRect& rect)
    {
        return rect.w > 0.0f && rect.h > 0.0f;
    }

    /**
     * @brief Retourne un rectangle "gonfle" autour d'un rectangle source.
     */
    SDL_FRect expandRect(const SDL_FRect& rect, float amountX, float amountY)
    {
        return SDL_FRect{
            rect.x - amountX,
            rect.y - amountY,
            rect.w + (amountX * 2.0f),
            rect.h + (amountY * 2.0f)};
    }

    /**
     * @brief Retourne un rectangle redimensionne autour de son centre.
     */
    SDL_FRect scaleRectFromCenter(const SDL_FRect& rect, float scaleX, float scaleY)
    {
        if (!isValidRect(rect))
        {
            return rect;
        }

        const float scaledWidth = rect.w * scaleX;
        const float scaledHeight = rect.h * scaleY;
        return SDL_FRect{
            std::round(rect.x + ((rect.w - scaledWidth) * 0.5f)),
            std::round(rect.y + ((rect.h - scaledHeight) * 0.5f)),
            scaledWidth,
            scaledHeight};
    }

    /**
     * @brief Fusionne deux rectangles logiques en un seul englobant.
     */
    SDL_FRect unionRect(const SDL_FRect& a, const SDL_FRect& b)
    {
        if (!isValidRect(a))
        {
            return b;
        }
        if (!isValidRect(b))
        {
            return a;
        }

        const float left = (std::min)(a.x, b.x);
        const float top = (std::min)(a.y, b.y);
        const float right = (std::max)(a.x + a.w, b.x + b.w);
        const float bottom = (std::max)(a.y + a.h, b.y + b.h);
        return SDL_FRect{left, top, right - left, bottom - top};
    }

    /**
     * @brief Retourne true si le point se trouve dans le rectangle.
     */
    bool pointInRect(const SDL_FRect& rect, float x, float y)
    {
        return isValidRect(rect) &&
               x >= rect.x &&
               y >= rect.y &&
               x <= (rect.x + rect.w) &&
               y <= (rect.y + rect.h);
    }

    /**
     * @brief Convertit un rectangle flottant en clip SDL entier.
     */
    SDL_Rect toClipRect(const SDL_FRect& rect)
    {
        return SDL_Rect{
            static_cast<int>(std::floor(rect.x)),
            static_cast<int>(std::floor(rect.y)),
            static_cast<int>(std::ceil(rect.w)),
            static_cast<int>(std::ceil(rect.h))};
    }

    /**
     * @brief Convertit la position souris fenetre vers l'espace render logique.
     */
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

    /**
     * @brief Met a jour le curseur systeme du menu sans recreer le curseur a chaque frame.
     */
    void applyMenuCursor(bool wantsPointer)
    {
        static SDL_Cursor* defaultCursor = nullptr;
        static SDL_Cursor* pointerCursor = nullptr;
        static bool lastPointerState = false;
        static bool initialized = false;

        if (!initialized)
        {
            defaultCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
            pointerCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
            initialized = true;
        }

        if (initialized && wantsPointer == lastPointerState)
        {
            return;
        }

        SDL_Cursor* targetCursor = wantsPointer ? pointerCursor : defaultCursor;
        if (targetCursor != nullptr)
        {
            rc2d_mouse_setCursor(targetCursor);
            lastPointerState = wantsPointer;
        }
    }

    /**
     * @brief Lit la taille d'une texture SDL de maniere sure.
     */
    bool getTextureSize(SDL_Texture* texture, float* outW, float* outH)
    {
        if (texture == nullptr)
        {
            return false;
        }

        float width = 0.0f;
        float height = 0.0f;
        if (!SDL_GetTextureSize(texture, &width, &height))
        {
            return false;
        }

        if (width <= 0.0f || height <= 0.0f)
        {
            return false;
        }

        if (outW != nullptr)
        {
            *outW = width;
        }
        if (outH != nullptr)
        {
            *outH = height;
        }
        return true;
    }

    /**
     * @brief Reproduit le placement RC2D_UI ancre + marges pour connaitre le rect courant.
     */
    SDL_FRect computeAnchoredImageRect(const RC2D_UIImage& uiImage)
    {
        if (!uiImage.visible || uiImage.image.sdl_texture == nullptr)
        {
            return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        }

        float width = 0.0f;
        float height = 0.0f;
        if (!getTextureSize(uiImage.image.sdl_texture, &width, &height))
        {
            return SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
        }

        const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
        float marginX = uiImage.margin_x;
        float marginY = uiImage.margin_y;
        if (uiImage.margin_mode == RC2D_UI_MARGIN_PERCENT)
        {
            marginX *= safeRect.w;
            marginY *= safeRect.h;
        }

        SDL_FRect result = SDL_FRect{0.0f, 0.0f, width, height};
        switch (uiImage.anchor)
        {
            case RC2D_UI_ANCHOR_TOP_LEFT:
                result.x = safeRect.x + marginX;
                result.y = safeRect.y + marginY;
                break;
            case RC2D_UI_ANCHOR_TOP_RIGHT:
                result.x = safeRect.x + safeRect.w - marginX - width;
                result.y = safeRect.y + marginY;
                break;
            case RC2D_UI_ANCHOR_BOTTOM_LEFT:
                result.x = safeRect.x + marginX;
                result.y = safeRect.y + safeRect.h - marginY - height;
                break;
            case RC2D_UI_ANCHOR_BOTTOM_RIGHT:
                result.x = safeRect.x + safeRect.w - marginX - width;
                result.y = safeRect.y + safeRect.h - marginY - height;
                break;
            case RC2D_UI_ANCHOR_BOTTOM_CENTER:
                result.x = safeRect.x + ((safeRect.w - width) * 0.5f) + marginX;
                result.y = safeRect.y + safeRect.h - marginY - height;
                break;
            case RC2D_UI_ANCHOR_CENTER:
                result.x = safeRect.x + ((safeRect.w - width) * 0.5f) + marginX;
                result.y = safeRect.y + ((safeRect.h - height) * 0.5f) + marginY;
                break;
            case RC2D_UI_ANCHOR_TOP_CENTER:
            default:
                result.x = safeRect.x + ((safeRect.w - width) * 0.5f) + marginX;
                result.y = safeRect.y + marginY;
                break;
        }

        result.x = std::round(result.x);
        result.y = std::round(result.y);
        return result;
    }

    /**
     * @brief Dessine une texture precise dans un rectangle destination avec alpha temporaire.
     */
    void drawImageToRectWithAlpha(RC2D_Image* image, const SDL_FRect& dstRect, float alpha01)
    {
        if (image == nullptr || image->sdl_texture == nullptr || !isValidRect(dstRect))
        {
            return;
        }

        SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
        if (renderer == nullptr)
        {
            return;
        }

        Uint8 previousAlpha = 255;
        SDL_GetTextureAlphaMod(image->sdl_texture, &previousAlpha);
        SDL_SetTextureBlendMode(image->sdl_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(
            image->sdl_texture,
            static_cast<Uint8>(std::round(std::clamp(alpha01, 0.0f, 1.0f) * 255.0f)));
        SDL_RenderTexture(renderer, image->sdl_texture, nullptr, &dstRect);
        SDL_SetTextureAlphaMod(image->sdl_texture, previousAlpha);
    }
}

double MenuScene::clamp01(double value)
{
    // Clamp lower bound.
    if (value < 0.0)
    {
        // Return minimum allowed value.
        return 0.0;
    }

    // Clamp upper bound.
    if (value > 1.0)
    {
        // Return maximum allowed value.
        return 1.0;
    }

    // Value is already valid.
    return value;
}

MenuScene::MenuScene(void)
    : loginBackgroundVideo{},
      loginBackgroundOpenAttempted(false),
      logoUi{},
      inputEmailUi{},
      inputPasswordUi{},
      buttonLoginUi{},
      languageFlags{},
      languageButtonRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageDropdownRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageListViewportRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageScrollTrackRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageScrollThumbRect{0.0f, 0.0f, 0.0f, 0.0f},
      loginCardRect{0.0f, 0.0f, 0.0f, 0.0f},
      hoveredLanguageButton(false),
      hoveredLanguageScrollbar(false),
      hoveredLanguageFlagIndex(-1),
      selectedLanguageFlagIndex(-1),
      languageDropdownOpen(false),
      languageScrollDragging(false),
      languageScrollOffset(0.0f),
      maxLanguageScrollOffset(0.0f),
      languageScrollDragOffsetY(0.0f),
      logoReveal{},
      panelReveal{},
      flagsReveal{},
      emailReveal{},
      passwordReveal{},
      buttonReveal{},
      menuMusic(nullptr),
      menuTrack(nullptr),
      menuMusicStarted(false),
      ambientAnimationTime(0.0),
      loginFadeAlpha(1.0f)
{
    // Constructor body intentionally empty.
}

void MenuScene::resetRevealAnimations(void)
{
    // Helper local pour initialiser chaque reveal sans dependre d'un temps global moteur.
    const auto initializeReveal =
        [](MenuRevealAnimation* animation,
           double delay,
           double duration,
           double startOffset,
           double endOffset,
           MenuEaseFunction offsetEaseFunction,
           MenuEaseFunction alphaEaseFunction)
        {
            if (animation == nullptr)
            {
                return;
            }

            animation->startDelay = delay;
            animation->remainingDelay = delay;
            animation->duration = duration;
            animation->elapsed = 0.0;
            animation->startOffset = startOffset;
            animation->endOffset = endOffset;
            animation->startAlpha = 0.0;
            animation->endAlpha = 1.0;
            animation->offsetEaseFunction = offsetEaseFunction;
            animation->alphaEaseFunction = alphaEaseFunction;
            animation->started = false;
            animation->currentOffset = static_cast<float>(startOffset);
            animation->currentAlpha = 0.0f;
        };

    // Le logo arrive d'abord avec une petite descente douce.
    initializeReveal(
        &this->logoReveal,
        0.00,
        0.92,
        -0.070,
        0.0,
        rc2d_tweening_easeOutCubic,
        rc2d_tweening_easeOutSine);

    // Ancien panneau central: on garde une animation neutre pour conserver le rythme global.
    initializeReveal(
        &this->panelReveal,
        0.18,
        0.88,
        0.0,
        0.0,
        rc2d_tweening_easeOutCubic,
        rc2d_tweening_easeOutSine);

    // Le selecteur de langue vient du haut un peu apres le logo.
    initializeReveal(
        &this->flagsReveal,
        0.26,
        0.76,
        -0.040,
        0.0,
        rc2d_tweening_easeOutCubic,
        rc2d_tweening_easeOutSine);

    // Les deux inputs partent depuis les bords puis reviennent au centre.
    initializeReveal(
        &this->emailReveal,
        0.30,
        0.90,
        -0.42,
        0.0,
        rc2d_tweening_easeOutBack,
        rc2d_tweening_easeOutSine);
    initializeReveal(
        &this->passwordReveal,
        0.42,
        0.92,
        0.42,
        0.0,
        rc2d_tweening_easeOutBack,
        rc2d_tweening_easeOutSine);

    // Le bouton termine l'arrivee avec un leger rebond pour le cote "game menu".
    initializeReveal(
        &this->buttonReveal,
        0.60,
        0.84,
        0.060,
        0.0,
        rc2d_tweening_easeOutBack,
        rc2d_tweening_easeOutSine);
}

void MenuScene::updateRevealAnimation(MenuRevealAnimation* animation, double dt)
{
    if (animation == nullptr)
    {
        return;
    }

    const double safeDt = std::clamp(dt, 0.0, 0.050);

    // Attendre le delai souhaite avant d'activer la progression du tween.
    if (!animation->started)
    {
        animation->remainingDelay -= safeDt;
        if (animation->remainingDelay > 0.0)
        {
            return;
        }
        animation->started = true;
        animation->remainingDelay = 0.0;
    }

    animation->elapsed = (std::min)(animation->elapsed + safeDt, animation->duration);

    double progress = 1.0;
    if (animation->duration > 0.0)
    {
        progress = std::clamp(animation->elapsed / animation->duration, 0.0, 1.0);
    }

    const double offsetRatio = (animation->offsetEaseFunction != nullptr)
        ? animation->offsetEaseFunction(progress)
        : progress;
    const double alphaRatio = (animation->alphaEaseFunction != nullptr)
        ? animation->alphaEaseFunction(progress)
        : progress;

    animation->currentOffset = static_cast<float>(
        menu_scene::lerp(animation->startOffset, animation->endOffset, offsetRatio));
    animation->currentAlpha = static_cast<float>(
        menu_scene::lerp(animation->startAlpha, animation->endAlpha, alphaRatio));
}

void MenuScene::drawFullscreenBlackWithAlpha(double alpha01)
{
    // Clamp alpha before conversion.
    const double alphaClamped = clamp01(alpha01);

    // Skip draw if fully transparent.
    if (alphaClamped <= 0.0)
    {
        return;
    }

    // Read visible safe rectangle from shared game screen wrapper.
    SDL_FRect rect = GetGameScreen().rect;

    // Guard invalid rectangle.
    if (rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    // Enable alpha blending for overlay pass.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    // Set black color and requested opacity.
    rc2d_graphics_setColor({0, 0, 0, static_cast<Uint8>(alphaClamped * 255.0)});

    // Draw fullscreen rectangle.
    rc2d_graphics_rectangle("fill", &rect);

    // Restore default blend mode.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void MenuScene::goToGameScene(void)
{
    // Switch scene if manager is available.
    if (sceneManager != nullptr)
    {
        sceneManager->changeScene("game");
    }
}

void MenuScene::syncAnimatedUiState(void)
{
    // Appliquer les tweens courants aux widgets principaux du menu.
    this->logoUi.margin_x = 0.0f;
    this->logoUi.margin_y = kLogoMarginY + this->logoReveal.currentOffset;
    this->inputEmailUi.margin_x = this->emailReveal.currentOffset;
    this->inputEmailUi.margin_y = kInputEmailMarginY;
    this->inputPasswordUi.margin_x = this->passwordReveal.currentOffset;
    this->inputPasswordUi.margin_y = kInputPasswordMarginY;
    this->buttonLoginUi.margin_x = 0.0f;
    this->buttonLoginUi.margin_y = kButtonLoginMarginY + this->buttonReveal.currentOffset;

    // Precalculer les rectangles pour les collisions et le layout de la frame.
    this->logoUi.last_drawn_rect = menu_scene::computeAnchoredImageRect(this->logoUi);
    this->inputEmailUi.last_drawn_rect = menu_scene::computeAnchoredImageRect(this->inputEmailUi);
    this->inputPasswordUi.last_drawn_rect = menu_scene::computeAnchoredImageRect(this->inputPasswordUi);

    const SDL_FRect loginButtonBaseRect = menu_scene::computeAnchoredImageRect(this->buttonLoginUi);
    const float pulseAmount =
        std::sin(static_cast<float>(this->ambientAnimationTime * menu_scene::kLoginButtonPulseSpeed)) *
        menu_scene::kLoginButtonPulseScaleAmplitude *
        static_cast<float>(this->clamp01(this->buttonReveal.currentAlpha));
    const float pulseScale = 1.0f + pulseAmount;
    this->buttonLoginUi.last_drawn_rect = menu_scene::scaleRectFromCenter(
        loginButtonBaseRect,
        pulseScale,
        pulseScale);
}

void MenuScene::rebuildLanguageSelectionFromContext(void)
{
    const ClientLanguageState& languageState = GetClientLanguageState();

    // Prioriser le drapeau courant actif, sinon retomber sur la detection systeme.
    std::string wantedFlagName = languageState.getCurrentFlagName();
    if (wantedFlagName.empty())
    {
        wantedFlagName = languageState.getDetectedFlagName();
    }
    if (wantedFlagName.empty())
    {
        wantedFlagName = "england";
    }

    this->selectedLanguageFlagIndex = -1;
    for (std::size_t index = 0; index < this->languageFlags.size(); ++index)
    {
        if (this->languageFlags[index].assetName == wantedFlagName)
        {
            this->selectedLanguageFlagIndex = static_cast<int>(index);
            return;
        }
    }

    // Garder un fallback stable meme si le drapeau voulu n'existe pas.
    if (!this->languageFlags.empty())
    {
        this->selectedLanguageFlagIndex = 0;
    }
}

void MenuScene::updateLanguageFlagLayout(void)
{
    this->languageButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageDropdownRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageListViewportRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollTrackRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollThumbRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->loginCardRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->maxLanguageScrollOffset = 0.0f;

    // Initialiser les rectangles de drapeau a vide avant recalcul.
    for (MenuLanguageFlagEntry& entry : this->languageFlags)
    {
        entry.drawRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }

    const int flagCount = static_cast<int>(this->languageFlags.size());
    if (flagCount <= 0)
    {
        return;
    }

    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    this->languageButtonRect = SDL_FRect{
        std::round(safeRect.x + safeRect.w - menu_scene::kLanguageButtonMargin - menu_scene::kLanguageButtonWidth),
        std::round(safeRect.y + menu_scene::kLanguageButtonMargin + (safeRect.h * this->flagsReveal.currentOffset)),
        menu_scene::kLanguageButtonWidth,
        menu_scene::kLanguageButtonHeight};

    const int visibleItems = (std::min)(flagCount, menu_scene::kLanguageVisibleItemCount);
    const float contentHeight =
        (flagCount * menu_scene::kLanguageListItemHeight) +
        ((flagCount - 1) * menu_scene::kLanguageListGapY);
    const float viewportHeight =
        (visibleItems * menu_scene::kLanguageListItemHeight) +
        ((visibleItems - 1) * menu_scene::kLanguageListGapY);
    this->maxLanguageScrollOffset = (std::max)(0.0f, contentHeight - viewportHeight);
    this->languageScrollOffset = std::clamp(this->languageScrollOffset, 0.0f, this->maxLanguageScrollOffset);

    if (this->languageDropdownOpen)
    {
        const float dropdownHeight = (menu_scene::kLanguageListPadding * 2.0f) + viewportHeight;
        this->languageDropdownRect = SDL_FRect{
            std::round(this->languageButtonRect.x),
            std::round(this->languageButtonRect.y + this->languageButtonRect.h + menu_scene::kLanguageDropdownGap),
            this->languageButtonRect.w,
            dropdownHeight};

        this->languageListViewportRect = SDL_FRect{
            this->languageDropdownRect.x + menu_scene::kLanguageListPadding,
            this->languageDropdownRect.y + menu_scene::kLanguageListPadding,
            this->languageDropdownRect.w - (menu_scene::kLanguageListPadding * 2.0f),
            viewportHeight};

        this->languageScrollTrackRect = SDL_FRect{
            this->languageDropdownRect.x + this->languageDropdownRect.w - menu_scene::kLanguageScrollBarWidth,
            this->languageDropdownRect.y + menu_scene::kLanguageScrollBarPadding,
            menu_scene::kLanguageScrollBarWidth,
            this->languageDropdownRect.h - (menu_scene::kLanguageScrollBarPadding * 2.0f)};

        for (int flagIndex = 0; flagIndex < flagCount; ++flagIndex)
        {
            this->languageFlags[flagIndex].drawRect = SDL_FRect{
                std::round(this->languageListViewportRect.x),
                std::round(
                    this->languageListViewportRect.y +
                    (flagIndex * (menu_scene::kLanguageListItemHeight + menu_scene::kLanguageListGapY)) -
                    this->languageScrollOffset),
                this->languageButtonRect.w,
                menu_scene::kLanguageListItemHeight};
        }

        if (this->maxLanguageScrollOffset > 0.0f && this->languageScrollTrackRect.h > 0.0f)
        {
            const float thumbHeight = (std::max)(
                menu_scene::kMinScrollThumbHeight,
                this->languageScrollTrackRect.h * (viewportHeight / contentHeight));
            const float thumbTravel = (std::max)(1.0f, this->languageScrollTrackRect.h - thumbHeight);
            const float scrollRatio = this->languageScrollOffset / this->maxLanguageScrollOffset;
            this->languageScrollThumbRect = SDL_FRect{
                this->languageScrollTrackRect.x,
                std::round(this->languageScrollTrackRect.y + (thumbTravel * scrollRatio)),
                this->languageScrollTrackRect.w,
                thumbHeight};
        }
        else
        {
            this->languageScrollThumbRect = this->languageScrollTrackRect;
        }
    }
}

void MenuScene::updateMenuInteractivity(void)
{
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    menu_scene::getMouseRenderPosition(&mouseX, &mouseY);

    this->hoveredLanguageButton = menu_scene::pointInRect(this->languageButtonRect, mouseX, mouseY);
    this->hoveredLanguageScrollbar =
        this->languageDropdownOpen &&
        this->maxLanguageScrollOffset > 0.0f &&
        menu_scene::pointInRect(this->languageScrollTrackRect, mouseX, mouseY);
    this->hoveredLanguageFlagIndex = -1;
    if (this->languageDropdownOpen && !this->hoveredLanguageScrollbar)
    {
        for (std::size_t index = 0; index < this->languageFlags.size(); ++index)
        {
            if (!menu_scene::pointInRect(this->languageFlags[index].drawRect, mouseX, mouseY))
            {
                continue;
            }

            if (!menu_scene::pointInRect(this->languageListViewportRect, mouseX, mouseY))
            {
                continue;
            }

            this->hoveredLanguageFlagIndex = static_cast<int>(index);
            break;
        }
    }

    // Le menu entier utilise une logique simple de main au survol des elements cliquables.
    const bool hoveringEmail = rc2d_collision_pointInUIImagePixelPerfect(&this->inputEmailUi, mouseX, mouseY);
    const bool hoveringPassword = rc2d_collision_pointInUIImagePixelPerfect(&this->inputPasswordUi, mouseX, mouseY);
    const bool hoveringLogin = menu_scene::pointInRect(this->buttonLoginUi.last_drawn_rect, mouseX, mouseY);
    const bool wantsPointer =
        this->hoveredLanguageButton ||
        this->hoveredLanguageScrollbar ||
        (this->hoveredLanguageFlagIndex >= 0) ||
        hoveringEmail ||
        hoveringPassword ||
        hoveringLogin;

    menu_scene::applyMenuCursor(wantsPointer);
}

void MenuScene::updateLanguageScrollbarDrag(void)
{
    if (!this->languageScrollDragging)
    {
        return;
    }

    if (!this->languageDropdownOpen ||
        this->maxLanguageScrollOffset <= 0.0f ||
        !rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
    {
        this->languageScrollDragging = false;
        return;
    }

    if (!menu_scene::isValidRect(this->languageScrollTrackRect) || !menu_scene::isValidRect(this->languageScrollThumbRect))
    {
        this->languageScrollDragging = false;
        return;
    }

    const float thumbTravel = (std::max)(1.0f, this->languageScrollTrackRect.h - this->languageScrollThumbRect.h);
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    menu_scene::getMouseRenderPosition(&mouseX, &mouseY);
    (void)mouseX;

    const float thumbTop = std::clamp(
        mouseY - this->languageScrollDragOffsetY,
        this->languageScrollTrackRect.y,
        this->languageScrollTrackRect.y + thumbTravel);
    const float scrollRatio = (thumbTop - this->languageScrollTrackRect.y) / thumbTravel;
    this->languageScrollOffset = scrollRatio * this->maxLanguageScrollOffset;
}

void MenuScene::scrollLanguageDropdown(float deltaPixels)
{
    if (!this->languageDropdownOpen || this->maxLanguageScrollOffset <= 0.0f)
    {
        return;
    }

    this->languageScrollOffset = std::clamp(
        this->languageScrollOffset + deltaPixels,
        0.0f,
        this->maxLanguageScrollOffset);
}

void MenuScene::snapLanguageScrollToSelection(void)
{
    if (this->selectedLanguageFlagIndex < 0 ||
        this->selectedLanguageFlagIndex >= static_cast<int>(this->languageFlags.size()))
    {
        return;
    }

    const int visibleItems = (std::min)(
        static_cast<int>(this->languageFlags.size()),
        menu_scene::kLanguageVisibleItemCount);
    const float viewportHeight =
        (visibleItems * menu_scene::kLanguageListItemHeight) +
        ((visibleItems - 1) * menu_scene::kLanguageListGapY);
    const float itemTop =
        this->selectedLanguageFlagIndex *
        (menu_scene::kLanguageListItemHeight + menu_scene::kLanguageListGapY);
    const float itemBottom = itemTop + menu_scene::kLanguageListItemHeight;

    if (itemTop < this->languageScrollOffset)
    {
        this->languageScrollOffset = itemTop;
    }
    else if (itemBottom > this->languageScrollOffset + viewportHeight)
    {
        this->languageScrollOffset = itemBottom - viewportHeight;
    }

    this->languageScrollOffset = std::clamp(
        this->languageScrollOffset,
        0.0f,
        this->maxLanguageScrollOffset);
}

void MenuScene::unload(void)
{
    // Close background video if open.
    rc2d_video_close(&this->loginBackgroundVideo);

    // Reset opening-attempt flag.
    this->loginBackgroundOpenAttempted = false;

    // Free logo texture.
    ResetStorageImageRef(&this->logoUi.image);

    // Free logo CPU image data.
    ResetStorageImageDataRef(&this->logoUi.imageData);

    // Free email texture.
    ResetStorageImageRef(&this->inputEmailUi.image);

    // Free email CPU image data.
    ResetStorageImageDataRef(&this->inputEmailUi.imageData);

    // Free password texture.
    ResetStorageImageRef(&this->inputPasswordUi.image);

    // Free password CPU image data.
    ResetStorageImageDataRef(&this->inputPasswordUi.imageData);

    // Free login button texture.
    ResetStorageImageRef(&this->buttonLoginUi.image);

    // Free login button CPU image data.
    ResetStorageImageDataRef(&this->buttonLoginUi.imageData);

    // Invalider les refs locales des drapeaux partages par le cache TITLE.
    for (MenuLanguageFlagEntry& entry : this->languageFlags)
    {
        ResetStorageImageRef(&entry.image);
    }
    this->languageFlags.clear();
    this->languageButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageDropdownRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageListViewportRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollTrackRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollThumbRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->loginCardRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->hoveredLanguageButton = false;
    this->hoveredLanguageScrollbar = false;
    this->hoveredLanguageFlagIndex = -1;
    this->selectedLanguageFlagIndex = -1;
    this->languageDropdownOpen = false;
    this->languageScrollDragging = false;
    this->languageScrollOffset = 0.0f;
    this->maxLanguageScrollOffset = 0.0f;
    this->languageScrollDragOffsetY = 0.0f;

    // Restore a neutral cursor when leaving the menu.
    menu_scene::applyMenuCursor(false);

    // Stop and destroy active track.
    if (this->menuTrack != nullptr)
    {
        // Stop playback first.
        rc2d_track_stop(this->menuTrack);

        // Destroy track resource.
        rc2d_track_destroy(this->menuTrack);

        // Clear pointer after destroy.
        this->menuTrack = nullptr;
    }

    // Destroy loaded audio resource.
    if (this->menuMusic != nullptr)
    {
        // Free audio asset.
        ResetStorageAudioRef(&this->menuMusic);
    }

    // Reset playback flag.
    this->menuMusicStarted = false;
    this->ambientAnimationTime = 0.0;

    // Reset intro fade and animation state for the next visit.
    this->loginFadeAlpha = 1.0f;
    this->resetRevealAnimations();

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Unloaded\n");
}

void MenuScene::load(void)
{
    // Show mouse cursor for menu interaction.
    rc2d_mouse_setVisible(true);
    menu_scene::applyMenuCursor(false);

    // Reset video struct to clean state.
    this->loginBackgroundVideo = {};

    // Reset open-attempt guard.
    this->loginBackgroundOpenAttempted = false;

    // Reset intro fade.
    this->loginFadeAlpha = 1.0f;

    // Mark music as not started.
    this->menuMusicStarted = false;
    this->ambientAnimationTime = 0.0;

    // Reset transient UI state.
    this->hoveredLanguageButton = false;
    this->hoveredLanguageScrollbar = false;
    this->hoveredLanguageFlagIndex = -1;
    this->selectedLanguageFlagIndex = -1;
    this->languageDropdownOpen = false;
    this->languageScrollDragging = false;
    this->languageScrollOffset = 0.0f;
    this->maxLanguageScrollOffset = 0.0f;
    this->languageScrollDragOffsetY = 0.0f;
    this->languageButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageDropdownRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageListViewportRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollTrackRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollThumbRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->loginCardRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};

    // Load logo texture.
    this->logoUi.image = LoadStorageImage("assets/images/ui-scene-menu/logo-st.png", RC2D_STORAGE_TITLE);

    // Load logo CPU data for potential pixel collision.
    this->logoUi.imageData = LoadStorageImageData("assets/images/ui-scene-menu/logo-st.png", RC2D_STORAGE_TITLE);

    // Anchor logo at top center.
    this->logoUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->logoUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // Start centered horizontally.
    this->logoUi.margin_x = 0.0f;

    // Start from the reference top position.
    this->logoUi.margin_y = kLogoMarginY;

    // Keep logo visible.
    this->logoUi.visible = true;

    // Logo is not interactable.
    this->logoUi.hittable = false;

    // Load email texture.
    this->inputEmailUi.image = LoadStorageImage("assets/images/ui-scene-menu/input-email.png", RC2D_STORAGE_TITLE);

    // Load email CPU data.
    this->inputEmailUi.imageData = LoadStorageImageData("assets/images/ui-scene-menu/input-email.png", RC2D_STORAGE_TITLE);

    // Anchor email at top center.
    this->inputEmailUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->inputEmailUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // Start centered horizontally.
    this->inputEmailUi.margin_x = 0.0f;

    // Vertical position for email field.
    this->inputEmailUi.margin_y = kInputEmailMarginY;

    // Keep email field visible.
    this->inputEmailUi.visible = true;

    // Enable interactions for email field.
    this->inputEmailUi.hittable = true;

    // Load password texture.
    this->inputPasswordUi.image = LoadStorageImage("assets/images/ui-scene-menu/input-password.png", RC2D_STORAGE_TITLE);

    // Load password CPU data.
    this->inputPasswordUi.imageData = LoadStorageImageData("assets/images/ui-scene-menu/input-password.png", RC2D_STORAGE_TITLE);

    // Anchor password at top center.
    this->inputPasswordUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->inputPasswordUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // Start centered horizontally.
    this->inputPasswordUi.margin_x = 0.0f;

    // Vertical position for password field.
    this->inputPasswordUi.margin_y = kInputPasswordMarginY;

    // Keep password field visible.
    this->inputPasswordUi.visible = true;

    // Enable interactions for password field.
    this->inputPasswordUi.hittable = true;

    // Load login button texture.
    this->buttonLoginUi.image = LoadStorageImage("assets/images/ui-scene-menu/button-login.png", RC2D_STORAGE_TITLE);

    // Load login button CPU data.
    this->buttonLoginUi.imageData = LoadStorageImageData("assets/images/ui-scene-menu/button-login.png", RC2D_STORAGE_TITLE);

    // Anchor button at top center.
    this->buttonLoginUi.anchor = RC2D_UI_ANCHOR_TOP_CENTER;

    // Use percentage margins.
    this->buttonLoginUi.margin_mode = RC2D_UI_MARGIN_PERCENT;

    // Start centered horizontally.
    this->buttonLoginUi.margin_x = 0.0f;

    // Vertical position for button.
    this->buttonLoginUi.margin_y = kButtonLoginMarginY;

    // Keep button visible.
    this->buttonLoginUi.visible = true;

    // Enable button hit-testing.
    this->buttonLoginUi.hittable = true;

    // Charger tous les drapeaux proposes par le menu.
    this->languageFlags.clear();
    this->languageFlags.reserve(menu_scene::kLanguageFlagNames.size());
    for (const char* flagName : menu_scene::kLanguageFlagNames)
    {
        MenuLanguageFlagEntry entry{};
        entry.assetName = flagName;

        // Chaque drapeau reste un asset partage TITLE pour eviter les doublons.
        std::string storagePath = "assets/images/languages/";
        storagePath += flagName;
        storagePath += ".png";
        entry.image = LoadStorageImage(storagePath.c_str(), RC2D_STORAGE_TITLE);
        this->languageFlags.push_back(entry);
    }

    // Initialiser les tweens d'arrivee avant le premier draw.
    this->resetRevealAnimations();
    this->syncAnimatedUiState();
    this->rebuildLanguageSelectionFromContext();
    this->updateLanguageFlagLayout();
    this->snapLanguageScrollToSelection();

    // Load menu music audio.
    this->menuMusic = LoadStorageAudio("assets/sounds/sound_menu.opus", RC2D_STORAGE_TITLE, true);

    // Handle audio load failure.
    if (this->menuMusic == nullptr)
    {
        // Log load issue.
        RC2D_log(RC2D_LOG_WARN, "Failed to load menu music: %s", SDL_GetError());
    }
    else
    {
        // Create track for music playback.
        this->menuTrack = rc2d_track_create();

        // Handle track creation failure.
        if (this->menuTrack == nullptr)
        {
            // Log track creation issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to create menu track: %s", SDL_GetError());
        }
        else if (!rc2d_track_setAudio(this->menuTrack, this->menuMusic))
        {
            // Log track binding issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to set menu track audio: %s", SDL_GetError());
        }
    }

    // Log lifecycle transition.
    RC2D_log(RC2D_LOG_INFO, "Menu Scene Loaded\n");
}

void MenuScene::update(double dt)
{
    // Animate intro fade until transparent.
    if (this->loginFadeAlpha > 0.0f)
    {
        // Subtract fade speed scaled by dt.
        this->loginFadeAlpha -= static_cast<float>(kLoginFadeSpeed * dt);

        // Clamp at zero.
        if (this->loginFadeAlpha < 0.0f)
        {
            this->loginFadeAlpha = 0.0f;
        }
    }

    // Start menu music once.
    if (!this->menuMusicStarted && this->menuTrack != nullptr && this->menuMusic != nullptr)
    {
        // Request looped playback.
        if (!rc2d_track_play(this->menuTrack, -1))
        {
            // Log playback issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to play menu music: %s", SDL_GetError());
        }
        else
        {
            // Mark as started to avoid replay spam.
            this->menuMusicStarted = true;
        }
    }

    // Try opening menu background video once.
    if (this->loginBackgroundVideo.format_ctx == nullptr && !this->loginBackgroundOpenAttempted)
    {
        // Mark attempt to avoid retry loops.
        this->loginBackgroundOpenAttempted = true;

        // Open menu background video.
        if (OpenStorageVideo(
                &this->loginBackgroundVideo,
                "assets/videos/background-menu-1080p.mp4",
                RC2D_STORAGE_TITLE) != 0)
        {
            // Log opening issue.
            RC2D_log(RC2D_LOG_WARN, "Failed to open menu background video: %s", SDL_GetError());
        }
        else
        {
            // Loop the menu background.
            rc2d_video_setLoop(&this->loginBackgroundVideo, 1);
        }
    }

    // Decode and advance background video when available.
    if (this->loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_update(&this->loginBackgroundVideo, dt);
    }

    this->ambientAnimationTime += std::clamp(dt, 0.0, 0.050);

    // Faire progresser toutes les animations d'entree du menu.
    this->updateRevealAnimation(&this->logoReveal, dt);
    this->updateRevealAnimation(&this->panelReveal, dt);
    this->updateRevealAnimation(&this->flagsReveal, dt);
    this->updateRevealAnimation(&this->emailReveal, dt);
    this->updateRevealAnimation(&this->passwordReveal, dt);
    this->updateRevealAnimation(&this->buttonReveal, dt);

    // Recalculer l'etat UI dynamique et les zones interactives du menu.
    this->syncAnimatedUiState();
    this->rebuildLanguageSelectionFromContext();
    this->updateLanguageFlagLayout();
    this->updateLanguageScrollbarDrag();
    this->updateLanguageFlagLayout();
    this->updateMenuInteractivity();
}

void MenuScene::drawLoginCard(void)
{
    if (!menu_scene::isValidRect(this->loginCardRect))
    {
        return;
    }

    const float alpha01 = static_cast<float>(this->clamp01(this->panelReveal.currentAlpha));
    if (alpha01 <= 0.0f)
    {
        return;
    }

    const SDL_FRect shadowRect = menu_scene::expandRect(this->loginCardRect, -6.0f, -6.0f);
    const SDL_FRect innerRect = menu_scene::expandRect(this->loginCardRect, -8.0f, -8.0f);
    const SDL_FRect headerRect = SDL_FRect{
        this->loginCardRect.x + 18.0f,
        this->loginCardRect.y + 16.0f,
        this->loginCardRect.w - 36.0f,
        36.0f};

    // Dessiner un fond multi-couches pour donner un rendu plus "jeu" au menu.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);

    SDL_FRect shadowDrawRect = shadowRect;
    shadowDrawRect.x += 12.0f;
    shadowDrawRect.y += 14.0f;
    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardShadow, alpha01));
    rc2d_graphics_rectangle("fill", &shadowDrawRect);

    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardFill, alpha01));
    rc2d_graphics_rectangle("fill", &this->loginCardRect);
    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardBorder, alpha01));
    rc2d_graphics_rectangle("line", &this->loginCardRect);

    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardInnerFill, alpha01));
    rc2d_graphics_rectangle("fill", &innerRect);
    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardInnerBorder, alpha01));
    rc2d_graphics_rectangle("line", &innerRect);

    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kHeaderFill, alpha01));
    rc2d_graphics_rectangle("fill", &headerRect);
    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kHeaderLine, alpha01));
    rc2d_graphics_rectangle("line", &headerRect);

    const float glowLineY = headerRect.y + headerRect.h + 9.0f;
    rc2d_graphics_line(
        this->loginCardRect.x + 24.0f,
        glowLineY,
        this->loginCardRect.x + this->loginCardRect.w - 24.0f,
        glowLineY);

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
}

void MenuScene::drawLanguageSelector(void)
{
    if (!menu_scene::isValidRect(this->languageButtonRect) || this->languageFlags.empty())
    {
        return;
    }

    const float alpha01 = static_cast<float>(this->clamp01(this->flagsReveal.currentAlpha));
    if (alpha01 <= 0.0f)
    {
        return;
    }

    const int selectedIndex =
        (this->selectedLanguageFlagIndex >= 0 &&
         this->selectedLanguageFlagIndex < static_cast<int>(this->languageFlags.size()))
            ? this->selectedLanguageFlagIndex
            : 0;
    MenuLanguageFlagEntry& selectedEntry = this->languageFlags[selectedIndex];
    const bool buttonActive = this->hoveredLanguageButton || this->languageDropdownOpen;
    const SDL_FRect buttonInnerRect = menu_scene::expandRect(this->languageButtonRect, -4.0f, -4.0f);
    SDL_FRect buttonShadowRect = this->languageButtonRect;
    buttonShadowRect.x += 5.0f;
    buttonShadowRect.y += 6.0f;

    const auto drawFlagInsideRect =
        [alpha01](MenuLanguageFlagEntry& entry, const SDL_FRect& containerRect, float insetX, float insetY, float alphaMultiplier)
        {
            float textureWidth = 0.0f;
            float textureHeight = 0.0f;
            if (!menu_scene::getTextureSize(entry.image.sdl_texture, &textureWidth, &textureHeight))
            {
                return;
            }

            const float maxWidth = containerRect.w - (insetX * 2.0f);
            const float maxHeight = containerRect.h - (insetY * 2.0f);
            if (maxWidth <= 0.0f || maxHeight <= 0.0f)
            {
                return;
            }

            const float textureScale = (std::min)(maxWidth / textureWidth, maxHeight / textureHeight);
            const SDL_FRect imageRect = SDL_FRect{
                std::round(containerRect.x + ((containerRect.w - (textureWidth * textureScale)) * 0.5f)),
                std::round(containerRect.y + ((containerRect.h - (textureHeight * textureScale)) * 0.5f)),
                textureWidth * textureScale,
                textureHeight * textureScale};

            menu_scene::drawImageToRectWithAlpha(
                &entry.image,
                imageRect,
                alpha01 * alphaMultiplier);
        };

    // Dessiner le bouton de langue compact en haut a droite.
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardShadow, alpha01 * 0.90f));
    rc2d_graphics_rectangle("fill", &buttonShadowRect);
    rc2d_graphics_setColor(
        menu_scene::applyAlpha(
            buttonActive ? menu_scene::kFlagHoverFill : menu_scene::kCardInnerFill,
            alpha01));
    rc2d_graphics_rectangle("fill", &this->languageButtonRect);
    rc2d_graphics_setColor(
        menu_scene::applyAlpha(
            buttonActive ? menu_scene::kFlagSelectedBorder : menu_scene::kCardBorder,
            alpha01));
    rc2d_graphics_rectangle("line", &this->languageButtonRect);
    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardInnerBorder, alpha01));
    rc2d_graphics_rectangle("line", &buttonInnerRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    drawFlagInsideRect(selectedEntry, this->languageButtonRect, 8.0f, 8.0f, 1.0f);

    if (!this->languageDropdownOpen || !menu_scene::isValidRect(this->languageDropdownRect))
    {
        return;
    }

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer != nullptr && menu_scene::isValidRect(this->languageListViewportRect))
    {
        const SDL_Rect clipRect = menu_scene::toClipRect(this->languageListViewportRect);
        SDL_SetRenderClipRect(renderer, &clipRect);
    }

    for (std::size_t index = 0; index < this->languageFlags.size(); ++index)
    {
        MenuLanguageFlagEntry& entry = this->languageFlags[index];
        if (!menu_scene::isValidRect(entry.drawRect))
        {
            continue;
        }

        if ((entry.drawRect.y + entry.drawRect.h) < this->languageListViewportRect.y ||
            entry.drawRect.y > (this->languageListViewportRect.y + this->languageListViewportRect.h))
        {
            continue;
        }

        const bool isSelected = static_cast<int>(index) == this->selectedLanguageFlagIndex;
        const bool isHovered = static_cast<int>(index) == this->hoveredLanguageFlagIndex;

        RC2D_Color fillColor = menu_scene::kFlagNormalFill;
        RC2D_Color borderColor = menu_scene::kFlagNormalBorder;
        if (isSelected)
        {
            fillColor = menu_scene::kFlagSelectedFill;
            borderColor = menu_scene::kFlagSelectedBorder;
        }
        else if (isHovered)
        {
            fillColor = menu_scene::kFlagHoverFill;
            borderColor = menu_scene::kFlagHoverBorder;
        }

        SDL_FRect itemShadowRect = entry.drawRect;
        itemShadowRect.x += 4.0f;
        itemShadowRect.y += 4.0f;
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardShadow, alpha01 * 0.72f));
        rc2d_graphics_rectangle("fill", &itemShadowRect);
        rc2d_graphics_setColor(menu_scene::applyAlpha(fillColor, alpha01));
        rc2d_graphics_rectangle("fill", &entry.drawRect);
        rc2d_graphics_setColor(menu_scene::applyAlpha(borderColor, alpha01));
        rc2d_graphics_rectangle("line", &entry.drawRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
        drawFlagInsideRect(
            entry,
            entry.drawRect,
            isSelected ? 8.0f : 10.0f,
            isSelected ? 8.0f : 10.0f,
            isSelected ? 1.0f : (isHovered ? 0.98f : 0.92f));
    }

    if (renderer != nullptr)
    {
        SDL_SetRenderClipRect(renderer, nullptr);
    }

    if (this->maxLanguageScrollOffset > 0.0f && menu_scene::isValidRect(this->languageScrollTrackRect))
    {
        const RC2D_Color thumbColor =
            (this->languageScrollDragging || this->hoveredLanguageScrollbar)
                ? menu_scene::kFlagHoverBorder
                : menu_scene::kFlagSelectedBorder;
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kFlagNormalFill, alpha01));
        rc2d_graphics_rectangle("fill", &this->languageScrollTrackRect);
        rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kCardInnerBorder, alpha01));
        rc2d_graphics_rectangle("line", &this->languageScrollTrackRect);
        rc2d_graphics_setColor(menu_scene::applyAlpha(thumbColor, alpha01));
        rc2d_graphics_rectangle("fill", &this->languageScrollThumbRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
    }
}

void MenuScene::drawUiImageWithAlpha(RC2D_UIImage* uiImage, float alpha01)
{
    if (uiImage == nullptr || uiImage->image.sdl_texture == nullptr || !uiImage->visible)
    {
        return;
    }

    SDL_FRect drawRect = uiImage->last_drawn_rect;
    if (!menu_scene::isValidRect(drawRect))
    {
        drawRect = menu_scene::computeAnchoredImageRect(*uiImage);
        uiImage->last_drawn_rect = drawRect;
    }

    const float clampedAlpha = static_cast<float>(this->clamp01(alpha01));
    if (clampedAlpha <= 0.0f)
    {
        return;
    }

    SDL_Renderer* renderer = SDL_GetRenderer(rc2d_window_getWindow());
    if (renderer == nullptr)
    {
        return;
    }

    Uint8 previousAlpha = 255;
    SDL_GetTextureAlphaMod(uiImage->image.sdl_texture, &previousAlpha);
    SDL_SetTextureBlendMode(uiImage->image.sdl_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(
        uiImage->image.sdl_texture,
        static_cast<Uint8>(std::round(clampedAlpha * 255.0f)));
    SDL_RenderTexture(renderer, uiImage->image.sdl_texture, nullptr, &drawRect);
    SDL_SetTextureAlphaMod(uiImage->image.sdl_texture, previousAlpha);
}

void MenuScene::draw(void)
{
    // Draw menu video if opened.
    if (this->loginBackgroundVideo.format_ctx != nullptr)
    {
        rc2d_video_draw(&this->loginBackgroundVideo);
    }

    // Dessiner directement les elements du menu sans panneau central.
    this->drawUiImageWithAlpha(&this->logoUi, this->logoReveal.currentAlpha);
    this->drawLanguageSelector();
    this->drawUiImageWithAlpha(&this->inputEmailUi, this->emailReveal.currentAlpha);
    this->drawUiImageWithAlpha(&this->inputPasswordUi, this->passwordReveal.currentAlpha);
    this->drawUiImageWithAlpha(&this->buttonLoginUi, this->buttonReveal.currentAlpha);

    // Draw intro fade if still active.
    if (this->loginFadeAlpha > 0.0f)
    {
        this->drawFullscreenBlackWithAlpha(this->loginFadeAlpha);
    }
}

void MenuScene::keypressed(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat,
    SDL_KeyboardID keyboardID)
{
    (void)key;
    (void)scancode;
    (void)mod;
    (void)isrepeat;
    (void)keyboardID;

    // Enter starts game.
    if (keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER)
    {
        this->goToGameScene();
        return;
    }
}

void MenuScene::mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID)
{
    (void)clicks;
    (void)mouseID;

    // Only process left click.
    if (button != RC2D_MOUSE_BUTTON_LEFT)
    {
        return;
    }

    if (menu_scene::pointInRect(this->languageButtonRect, x, y))
    {
        this->languageDropdownOpen = !this->languageDropdownOpen;
        this->languageScrollDragging = false;
        if (this->languageDropdownOpen)
        {
            this->snapLanguageScrollToSelection();
        }
        this->updateLanguageFlagLayout();
        this->updateMenuInteractivity();
        return;
    }

    if (this->languageDropdownOpen)
    {
        if (this->maxLanguageScrollOffset > 0.0f && menu_scene::pointInRect(this->languageScrollTrackRect, x, y))
        {
            this->languageScrollDragging = true;

            if (menu_scene::pointInRect(this->languageScrollThumbRect, x, y))
            {
                this->languageScrollDragOffsetY = y - this->languageScrollThumbRect.y;
            }
            else
            {
                this->languageScrollDragOffsetY = this->languageScrollThumbRect.h * 0.5f;
                const float thumbTravel = (std::max)(1.0f, this->languageScrollTrackRect.h - this->languageScrollThumbRect.h);
                const float thumbTop = std::clamp(
                    y - this->languageScrollDragOffsetY,
                    this->languageScrollTrackRect.y,
                    this->languageScrollTrackRect.y + thumbTravel);
                const float scrollRatio = (thumbTop - this->languageScrollTrackRect.y) / thumbTravel;
                this->languageScrollOffset = scrollRatio * this->maxLanguageScrollOffset;
                this->updateLanguageFlagLayout();
            }

            this->updateMenuInteractivity();
            return;
        }

        for (std::size_t index = 0; index < this->languageFlags.size(); ++index)
        {
            if (!menu_scene::pointInRect(this->languageListViewportRect, x, y) ||
                !menu_scene::pointInRect(this->languageFlags[index].drawRect, x, y))
            {
                continue;
            }

            // Memoriser le drapeau actif globalement pour les futures UIs.
            GetClientLanguageState().setCurrentFlagName(this->languageFlags[index].assetName);
            this->selectedLanguageFlagIndex = static_cast<int>(index);
            this->languageDropdownOpen = false;
            this->languageScrollDragging = false;
            this->snapLanguageScrollToSelection();
            this->updateLanguageFlagLayout();
            this->updateMenuInteractivity();

            RC2D_log(
                RC2D_LOG_INFO,
                "Selected menu language flag: %s",
                this->languageFlags[index].assetName.c_str());
            return;
        }

        if (menu_scene::pointInRect(this->languageDropdownRect, x, y))
        {
            return;
        }

        this->languageDropdownOpen = false;
        this->languageScrollDragging = false;
        this->updateLanguageFlagLayout();
        this->updateMenuInteractivity();
    }

    // Check email box hit.
    if (rc2d_collision_pointInUIImagePixelPerfect(&this->inputEmailUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked EMAIL input box");
    }
    // Check password box hit.
    else if (rc2d_collision_pointInUIImagePixelPerfect(&this->inputPasswordUi, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked PASSWORD input box");
    }
    // Check login button hit.
    else if (menu_scene::pointInRect(this->buttonLoginUi.last_drawn_rect, x, y))
    {
        RC2D_log(RC2D_LOG_INFO, "Clicked LOGIN button");
    }
}

void MenuScene::mousewheelmoved(
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
    (void)integer_x;
    (void)mouseID;

    if (!this->languageDropdownOpen || !menu_scene::pointInRect(this->languageListViewportRect, mouse_x, mouse_y))
    {
        return;
    }

    int delta = static_cast<int>(integer_y);
    if (delta == 0)
    {
        if (y > 0.0f)
        {
            delta = 1;
        }
        else if (y < 0.0f)
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

    if (delta != 0)
    {
        this->scrollLanguageDropdown(-(static_cast<float>(delta) * menu_scene::kLanguageScrollWheelStep));
        this->updateLanguageFlagLayout();
        this->updateMenuInteractivity();
    }
}
