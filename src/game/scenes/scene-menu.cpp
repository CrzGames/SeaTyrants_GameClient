#include "game/scenes/scene-menu.h"

#include "game/assets/title-asset-cache.h"

#include "core/context.h"
#include "game/scenes/scene-manager.h"
#include "game/ui/text-input-shortcuts.h"

#include <RC2D/RC2D_system.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <mutex>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

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
    static constexpr float kScrollThumbWheelHighlightSec = 0.25f;
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
    static constexpr RC2D_Color kErrorText = RC2D_Color{255, 129, 113, 255};
    static constexpr RC2D_Color kSuccessText = RC2D_Color{161, 220, 155, 255};
    static constexpr RC2D_Color kOverlayDim = RC2D_Color{0, 0, 0, 160};
    static constexpr RC2D_Color kServerRowFill = RC2D_Color{10, 20, 32, 214};
    static constexpr RC2D_Color kServerRowBorder = RC2D_Color{110, 146, 170, 182};
    static constexpr RC2D_Color kServerOpenText = RC2D_Color{161, 220, 155, 255};
    static constexpr RC2D_Color kServerClosedText = RC2D_Color{255, 129, 113, 255};
    static constexpr RC2D_Color kServerButtonFill = RC2D_Color{24, 48, 70, 226};
    static constexpr RC2D_Color kServerButtonHoverFill = RC2D_Color{43, 72, 98, 238};
    static constexpr RC2D_Color kServerButtonBorder = RC2D_Color{226, 191, 91, 255};
    static constexpr RC2D_Color kPendingPanelFill = RC2D_Color{9, 19, 31, 234};
    static constexpr RC2D_Color kPendingPanelBorder = RC2D_Color{236, 201, 102, 255};
    static constexpr RC2D_Color kPendingLoaderColor = RC2D_Color{211, 223, 236, 255};

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
    void applyMenuCursor(SDL_SystemCursor cursorId)
    {
        static SDL_SystemCursor lastCursorId = static_cast<SDL_SystemCursor>(-1);
        static SDL_Cursor* cachedCursors[3] = {nullptr, nullptr, nullptr};
        const int cacheIndex =
            (cursorId == SDL_SYSTEM_CURSOR_POINTER) ? 1 :
            (cursorId == SDL_SYSTEM_CURSOR_TEXT) ? 2 : 0;

        if (cursorId == lastCursorId)
        {
            return;
        }

        if (cachedCursors[cacheIndex] == nullptr)
        {
            cachedCursors[cacheIndex] = SDL_CreateSystemCursor(cursorId);
        }

        SDL_Cursor* targetCursor = cachedCursors[cacheIndex];
        if (targetCursor != nullptr)
        {
            rc2d_mouse_setCursor(targetCursor);
            lastCursorId = cursorId;
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

    /**
     * @brief Mesure la largeur d'un texte avec la police indiquee.
     */
    float measureTextWidth(RC2D_Font* font, const std::string& text)
    {
        if (font == nullptr || font->sdl_font == nullptr || text.empty())
        {
            return 0.0f;
        }

        RC2D_Text renderedText = rc2d_graphics_createText(font, text.c_str());
        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);
        rc2d_graphics_destroyText(&renderedText);
        return static_cast<float>(textWidth);
    }

    /**
     * @brief Dessine un texte aligne a gauche et centre verticalement dans un rectangle.
     */
    void drawTextLeftCenteredY(
        RC2D_Font* font,
        const std::string& text,
        const SDL_FRect& rect,
        float leftX,
        RC2D_Color color)
    {
        if (font == nullptr || font->sdl_font == nullptr || text.empty() || !isValidRect(rect))
        {
            return;
        }

        RC2D_Text renderedText = rc2d_graphics_createText(font, text.c_str());
        renderedText.color = color;
        rc2d_graphics_setTextColor(&renderedText);

        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);
        (void)textWidth;

        const float drawX = std::round(leftX);
        const float drawY = std::round(rect.y + ((rect.h - static_cast<float>(textHeight)) * 0.5f));
        rc2d_graphics_drawText(&renderedText, drawX, drawY);
        rc2d_graphics_destroyText(&renderedText);
    }

    /**
     * @brief Dessine un texte centre dans un rectangle.
     */
    void drawCenteredText(RC2D_Font* font, const std::string& text, const SDL_FRect& rect, RC2D_Color color)
    {
        if (font == nullptr || font->sdl_font == nullptr || text.empty() || !isValidRect(rect))
        {
            return;
        }

        RC2D_Text renderedText = rc2d_graphics_createText(font, text.c_str());
        renderedText.color = color;
        rc2d_graphics_setTextColor(&renderedText);

        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);

        const float drawX = std::round(rect.x + ((rect.w - static_cast<float>(textWidth)) * 0.5f));
        const float drawY = std::round(rect.y + ((rect.h - static_cast<float>(textHeight)) * 0.5f));
        rc2d_graphics_drawText(&renderedText, drawX, drawY);
        rc2d_graphics_destroyText(&renderedText);
    }

    /**
     * @brief Dessine un texte aligne a droite et en bas dans un rectangle (marges interieures).
     */
    void drawTextRightBottom(
        RC2D_Font* font,
        const std::string& text,
        const SDL_FRect& area,
        float marginRight,
        float marginBottom,
        RC2D_Color color)
    {
        if (font == nullptr || font->sdl_font == nullptr || text.empty() || !isValidRect(area))
        {
            return;
        }

        RC2D_Text renderedText = rc2d_graphics_createText(font, text.c_str());
        renderedText.color = color;
        rc2d_graphics_setTextColor(&renderedText);

        int textWidth = 0;
        int textHeight = 0;
        rc2d_graphics_getTextSize(&renderedText, &textWidth, &textHeight);

        const float drawX = std::round(
            area.x + area.w - marginRight - static_cast<float>(textWidth));
        const float drawY = std::round(
            area.y + area.h - marginBottom - static_cast<float>(textHeight));
        rc2d_graphics_drawText(&renderedText, drawX, drawY);
        rc2d_graphics_destroyText(&renderedText);
    }

    bool isUtf8ContinuationByte(unsigned char byteValue)
    {
        return (byteValue & 0xC0U) == 0x80U;
    }

    /**
     * @brief Retourne la version masquee d'un mot de passe pour l'affichage menu.
     */
    std::string buildMaskedPassword(const std::string& password)
    {
        std::size_t codepointCount = 0U;
        for (std::size_t index = 0U; index < password.size();)
        {
            ++codepointCount;
            ++index;
            while (index < password.size() &&
                   isUtf8ContinuationByte(static_cast<unsigned char>(password[index])))
            {
                ++index;
            }
        }

        return std::string(codepointCount, '*');
    }

    void eraseLastUtf8Codepoint(std::string* text)
    {
        if (text == nullptr || text->empty())
        {
            return;
        }

        std::size_t newSize = text->size() - 1U;
        while (newSize > 0U && isUtf8ContinuationByte(static_cast<unsigned char>((*text)[newSize])))
        {
            --newSize;
        }

        text->erase(newSize);
    }

    std::vector<std::size_t> buildUtf8CodepointOffsets(const std::string& text)
    {
        std::vector<std::size_t> offsets{};
        offsets.reserve(text.size() + 1U);
        offsets.push_back(0U);

        std::size_t index = 0U;
        while (index < text.size())
        {
            ++index;
            while (index < text.size() &&
                   isUtf8ContinuationByte(static_cast<unsigned char>(text[index])))
            {
                ++index;
            }

            offsets.push_back(index);
        }

        return offsets;
    }

    std::size_t clampByteOffsetToUtf8Boundary(const std::vector<std::size_t>& offsets, std::size_t byteOffset)
    {
        if (offsets.empty())
        {
            return 0U;
        }

        std::size_t clamped = (std::min)(byteOffset, offsets.back());
        for (std::size_t offset : offsets)
        {
            if (offset == clamped)
            {
                return offset;
            }

            if (offset > clamped)
            {
                break;
            }
        }

        std::size_t previousOffset = 0U;
        for (std::size_t offset : offsets)
        {
            if (offset > clamped)
            {
                break;
            }

            previousOffset = offset;
        }

        return previousOffset;
    }

    std::size_t findCodepointIndexForByteOffset(const std::vector<std::size_t>& offsets, std::size_t byteOffset)
    {
        if (offsets.empty())
        {
            return 0U;
        }

        const std::size_t clampedOffset = clampByteOffsetToUtf8Boundary(offsets, byteOffset);
        for (std::size_t index = 0U; index < offsets.size(); ++index)
        {
            if (offsets[index] == clampedOffset)
            {
                return index;
            }
        }

        return offsets.size() - 1U;
    }

    std::size_t findDisplayCaretIndexForX(
        RC2D_Font* font,
        const std::string& displayText,
        const std::vector<std::size_t>& displayOffsets,
        float textStartX,
        float mouseX)
    {
        if (displayOffsets.empty())
        {
            return 0U;
        }

        const std::size_t codepointCount = displayOffsets.size() - 1U;
        for (std::size_t index = 0U; index < codepointCount; ++index)
        {
            const float prefixWidth =
                menu_scene::measureTextWidth(font, displayText.substr(0U, displayOffsets[index]));
            const float nextPrefixWidth =
                menu_scene::measureTextWidth(font, displayText.substr(0U, displayOffsets[index + 1U]));
            const float midpointX = textStartX + prefixWidth + ((nextPrefixWidth - prefixWidth) * 0.5f);
            if (mouseX < midpointX)
            {
                return index;
            }
        }

        return codepointCount;
    }

    std::string sanitizeTextInput(const char* text)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return {};
        }

        std::string sanitized{};
        for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0U; ++cursor)
        {
            if ((*cursor < 32U || *cursor == 127U) && *cursor != ' ')
            {
                continue;
            }

            sanitized.push_back(static_cast<char>(*cursor));
        }

        return sanitized;
    }

    bool appendSanitizedTextWithLimit(std::string* destination, const char* text, std::size_t maxBytes)
    {
        if (destination == nullptr)
        {
            return false;
        }

        const std::string sanitized = sanitizeTextInput(text);
        if (sanitized.empty())
        {
            return false;
        }

        if (destination->size() + sanitized.size() > maxBytes)
        {
            return false;
        }

        destination->append(sanitized);
        return true;
    }

    /**
     * @brief Retourne une copie trimmee en debut/fin pour la validation locale.
     */
    std::string trimCopy(const std::string& text)
    {
        std::size_t startIndex = 0U;
        while (startIndex < text.size() &&
               std::isspace(static_cast<unsigned char>(text[startIndex])) != 0)
        {
            ++startIndex;
        }

        std::size_t endIndex = text.size();
        while (endIndex > startIndex &&
               std::isspace(static_cast<unsigned char>(text[endIndex - 1U])) != 0)
        {
            --endIndex;
        }

        return text.substr(startIndex, endIndex - startIndex);
    }

    std::string buildAuthFeedbackFailureTitle(long httpStatusCode, const std::string& code)
    {
        if (httpStatusCode == 401)
        {
            return "Connexion refusee";
        }

        if (httpStatusCode == 403)
        {
            return "Acces refuse";
        }

        if (httpStatusCode == 422)
        {
            return "Informations invalides";
        }

        if (httpStatusCode == 429)
        {
            return "Trop de tentatives";
        }

        if (code == "E_BACKEND_TIMEOUT")
        {
            return "Delai depasse";
        }

        if (code == "E_BACKEND_UNAVAILABLE" ||
            code == "E_BACKEND_SERVER" ||
            code == "E_BACKEND_NETWORK")
        {
            return "Backend indisponible";
        }

        return "Echec de connexion";
    }

    std::string buildAuthFeedbackDetailText(long httpStatusCode, const std::string& code)
    {
        std::string detailText{};
        if (!code.empty())
        {
            detailText = code;
        }

        if (httpStatusCode != 0)
        {
            if (!detailText.empty())
            {
                detailText += "  |  ";
            }
            detailText += "HTTP " + std::to_string(httpStatusCode);
        }

        if (detailText.empty())
        {
            detailText = "Aucun status HTTP exploitable.";
        }

        return detailText;
    }

    std::string buildAuthFeedbackHintText(long httpStatusCode, const std::string& code)
    {
        if (httpStatusCode == 401 || httpStatusCode == 403 || httpStatusCode == 422)
        {
            return "Verifiez les identifiants et les droits du compte puis reessayez.";
        }

        if (httpStatusCode == 429)
        {
            return "Patientez un instant avant une nouvelle tentative.";
        }

        if (code == "E_BACKEND_TIMEOUT")
        {
            return "Verifiez que le backend repond bien, puis relancez la connexion.";
        }

#if GAME_ENV_DEV
        return "Mode DEV: verifiez que le backend local HTTP est demarre puis reessayez.";
#else
        return "Verifiez la disponibilite du service puis reessayez.";
#endif
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
      menuTitleFont{},
      menuBodyFont{},
      loginEmailValue{},
      loginPasswordValue{},
      loginEmailFieldErrorMessage{},
      loginPasswordFieldErrorMessage{},
      focusedLoginField(MenuLoginFocusedField::NONE),
      menuLocalStatusMessage{},
      menuLocalStatusIsError(false),
      authUiSnapshot{},
      authFeedbackPopupVisible(false),
      loginEmailSelectionState{},
      loginPasswordSelectionState{},
      languageFlags{},
      languageButtonRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageDropdownRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageListViewportRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageScrollTrackRect{0.0f, 0.0f, 0.0f, 0.0f},
      languageScrollThumbRect{0.0f, 0.0f, 0.0f, 0.0f},
      loginCardRect{0.0f, 0.0f, 0.0f, 0.0f},
      authFeedbackPopupRect{0.0f, 0.0f, 0.0f, 0.0f},
      authFeedbackPopupActionButtonRect{0.0f, 0.0f, 0.0f, 0.0f},
      serverSelectionOverlayRect{0.0f, 0.0f, 0.0f, 0.0f},
      serverSelectionHeaderRect{0.0f, 0.0f, 0.0f, 0.0f},
      serverSelectionRowRects{},
      serverSelectionButtonRects{},
      hoveredLanguageButton(false),
      hoveredLanguageScrollbar(false),
      hoveredAuthFeedbackPopupActionButton(false),
      hoveredLanguageFlagIndex(-1),
      selectedLanguageFlagIndex(-1),
      hoveredServerSelectionButtonIndex(-1),
      languageDropdownOpen(false),
      languageScrollDragging(false),
      languageScrollWheelHighlightSec(0.0f),
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
    float pulseAmount = 0.0f;
    if (!this->authUiSnapshot.signInRequestPending)
    {
        pulseAmount =
            std::sin(static_cast<float>(this->ambientAnimationTime * menu_scene::kLoginButtonPulseSpeed)) *
            menu_scene::kLoginButtonPulseScaleAmplitude *
            static_cast<float>(this->clamp01(this->buttonReveal.currentAlpha));
    }
    const float pulseScale = 1.0f + pulseAmount;
    this->buttonLoginUi.last_drawn_rect = menu_scene::scaleRectFromCenter(
        loginButtonBaseRect,
        pulseScale,
        pulseScale);
}

void MenuScene::refreshAuthUiSnapshotFromNetworkState(void)
{
    // Lire une copie locale du resultat signin pour dessiner l'UI sans garder
    // le mutex partage pendant toute la phase de rendu.
    NetworkState& networkState = GetNetworkState();
    std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

    this->authUiSnapshot.signInRequestPending = networkState.authSignInRequestPending;
    this->authUiSnapshot.signInLastRequestSucceeded = networkState.authSignInLastRequestSucceeded;
    this->authUiSnapshot.serverSelectionVisible = networkState.authSignInServerSelectionVisible;
    this->authUiSnapshot.lastHttpStatusCode = networkState.authSignInLastHttpStatusCode;
    this->authUiSnapshot.lastCode = networkState.authSignInLastCode;
    this->authUiSnapshot.lastMessage = networkState.authSignInLastMessage;
    this->authUiSnapshot.servers = networkState.authSignInAvailableServers;
}

void MenuScene::rebuildAuthFeedbackOverlayLayout(void)
{
    this->authFeedbackPopupRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->authFeedbackPopupActionButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->hoveredAuthFeedbackPopupActionButton = false;

    if (!this->authFeedbackPopupVisible)
    {
        return;
    }

    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    const bool isPending = this->authUiSnapshot.signInRequestPending;
    const float panelWidth = std::clamp(safeRect.w - 40.0f, 340.0f, 620.0f);
    const float targetPanelHeight = isPending ? 205.0f : 300.0f;
    const float panelHeight = std::clamp(safeRect.h - 40.0f, 180.0f, targetPanelHeight);
    this->authFeedbackPopupRect = SDL_FRect{
        std::round(safeRect.x + ((safeRect.w - panelWidth) * 0.5f)),
        std::round(safeRect.y + ((safeRect.h - panelHeight) * 0.5f)),
        panelWidth,
        panelHeight};

    if (!isPending)
    {
        this->authFeedbackPopupActionButtonRect = SDL_FRect{
            std::round(this->authFeedbackPopupRect.x + ((this->authFeedbackPopupRect.w - 160.0f) * 0.5f)),
            std::round(this->authFeedbackPopupRect.y + this->authFeedbackPopupRect.h - 60.0f),
            160.0f,
            36.0f};
    }
}

void MenuScene::rebuildServerSelectionLayout(void)
{
    // Reinitialiser les rectangles a vide quand l'overlay n'est pas actif.
    this->serverSelectionOverlayRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionHeaderRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionRowRects.clear();
    this->serverSelectionButtonRects.clear();
    this->hoveredServerSelectionButtonIndex = -1;

    // Rien a dessiner si aucun overlay serveur n'est demande.
    if (!this->authUiSnapshot.serverSelectionVisible)
    {
        return;
    }

    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    const float panelWidth = (std::min)(960.0f, safeRect.w * 0.82f);
    const float rowHeight = 56.0f;
    const float rowGap = 10.0f;
    const float panelPadding = 18.0f;
    const float headerHeight = 46.0f;
    const float tableHeaderHeight = 28.0f;
    const std::size_t rowCount = this->authUiSnapshot.servers.size();
    const float bodyHeight =
        rowCount > 0U
            ? (static_cast<float>(rowCount) * rowHeight) + (static_cast<float>(rowCount - 1U) * rowGap)
            : rowHeight;
    const float panelHeight = (std::min)(safeRect.h * 0.78f, 132.0f + tableHeaderHeight + bodyHeight);

    // Centrer le panneau principal au-dessus du menu.
    this->serverSelectionOverlayRect = SDL_FRect{
        std::round(safeRect.x + ((safeRect.w - panelWidth) * 0.5f)),
        std::round(safeRect.y + ((safeRect.h - panelHeight) * 0.5f)),
        panelWidth,
        panelHeight};

    // Reserver un bandeau titre fixe pour presenter la liste des serveurs.
    this->serverSelectionHeaderRect = SDL_FRect{
        this->serverSelectionOverlayRect.x + panelPadding,
        this->serverSelectionOverlayRect.y + 14.0f,
        this->serverSelectionOverlayRect.w - (panelPadding * 2.0f),
        headerHeight};

    // Precalculer toutes les lignes et tous les boutons de connexion.
    this->serverSelectionRowRects.resize(rowCount);
    this->serverSelectionButtonRects.resize(rowCount);
    float currentRowY = this->serverSelectionHeaderRect.y + this->serverSelectionHeaderRect.h + tableHeaderHeight + 26.0f;
    for (std::size_t rowIndex = 0U; rowIndex < rowCount; ++rowIndex)
    {
        this->serverSelectionRowRects[rowIndex] = SDL_FRect{
            this->serverSelectionOverlayRect.x + panelPadding,
            std::round(currentRowY),
            this->serverSelectionOverlayRect.w - (panelPadding * 2.0f),
            rowHeight};

        this->serverSelectionButtonRects[rowIndex] = SDL_FRect{
            std::round(this->serverSelectionRowRects[rowIndex].x + this->serverSelectionRowRects[rowIndex].w - 150.0f),
            std::round(this->serverSelectionRowRects[rowIndex].y + 9.0f),
            132.0f,
            this->serverSelectionRowRects[rowIndex].h - 18.0f};

        currentRowY += rowHeight + rowGap;
    }
}

bool MenuScene::appendCharacterToEmailField(char character)
{
    const char text[2] = {character, '\0'};
    return this->insertTextIntoLoginField(MenuLoginFocusedField::EMAIL, text);
}

bool MenuScene::appendCharacterToPasswordField(char character)
{
    const char text[2] = {character, '\0'};
    return this->insertTextIntoLoginField(MenuLoginFocusedField::PASSWORD, text);
}

std::string* MenuScene::getLoginFieldValue(MenuLoginFocusedField field)
{
    switch (field)
    {
        case MenuLoginFocusedField::EMAIL:
            return &this->loginEmailValue;

        case MenuLoginFocusedField::PASSWORD:
            return &this->loginPasswordValue;

        case MenuLoginFocusedField::NONE:
        default:
            return nullptr;
    }
}

const std::string* MenuScene::getLoginFieldValue(MenuLoginFocusedField field) const
{
    switch (field)
    {
        case MenuLoginFocusedField::EMAIL:
            return &this->loginEmailValue;

        case MenuLoginFocusedField::PASSWORD:
            return &this->loginPasswordValue;

        case MenuLoginFocusedField::NONE:
        default:
            return nullptr;
    }
}

MenuScene::MenuLoginTextSelectionState* MenuScene::getLoginFieldSelectionState(MenuLoginFocusedField field)
{
    switch (field)
    {
        case MenuLoginFocusedField::EMAIL:
            return &this->loginEmailSelectionState;

        case MenuLoginFocusedField::PASSWORD:
            return &this->loginPasswordSelectionState;

        case MenuLoginFocusedField::NONE:
        default:
            return nullptr;
    }
}

const MenuScene::MenuLoginTextSelectionState* MenuScene::getLoginFieldSelectionState(MenuLoginFocusedField field) const
{
    switch (field)
    {
        case MenuLoginFocusedField::EMAIL:
            return &this->loginEmailSelectionState;

        case MenuLoginFocusedField::PASSWORD:
            return &this->loginPasswordSelectionState;

        case MenuLoginFocusedField::NONE:
        default:
            return nullptr;
    }
}

bool MenuScene::buildLoginFieldTextView(MenuLoginFocusedField field, MenuLoginFieldTextView* outView) const
{
    if (outView == nullptr)
    {
        return false;
    }

    const std::string* rawText = this->getLoginFieldValue(field);
    if (rawText == nullptr)
    {
        *outView = MenuLoginFieldTextView{};
        return false;
    }

    outView->sourceRect =
        (field == MenuLoginFocusedField::EMAIL)
            ? this->inputEmailUi.last_drawn_rect
            : this->inputPasswordUi.last_drawn_rect;
    if (!menu_scene::isValidRect(outView->sourceRect))
    {
        *outView = MenuLoginFieldTextView{};
        return false;
    }

    outView->textRect = SDL_FRect{
        outView->sourceRect.x + 34.0f,
        outView->sourceRect.y + 18.0f,
        outView->sourceRect.w - 68.0f,
        outView->sourceRect.h - 30.0f};
    outView->textStartX = outView->textRect.x + 42.0f;
    outView->rawOffsets = menu_scene::buildUtf8CodepointOffsets(*rawText);
    outView->displayText =
        (field == MenuLoginFocusedField::PASSWORD)
            ? menu_scene::buildMaskedPassword(*rawText)
            : *rawText;
    outView->displayOffsets = menu_scene::buildUtf8CodepointOffsets(outView->displayText);
    return true;
}

void MenuScene::collapseLoginFieldSelection(MenuLoginFocusedField field, std::size_t caretByte)
{
    std::string* rawText = this->getLoginFieldValue(field);
    MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
    if (rawText == nullptr || selectionState == nullptr)
    {
        return;
    }

    const std::vector<std::size_t> offsets = menu_scene::buildUtf8CodepointOffsets(*rawText);
    const std::size_t clampedCaret = menu_scene::clampByteOffsetToUtf8Boundary(offsets, caretByte);
    selectionState->anchorByte = clampedCaret;
    selectionState->caretByte = clampedCaret;
    selectionState->dragging = false;
}

void MenuScene::stopLoginTextSelectionDrag(void)
{
    this->loginEmailSelectionState.dragging = false;
    this->loginPasswordSelectionState.dragging = false;
}

void MenuScene::clearAllLoginTextSelections(void)
{
    this->collapseLoginFieldSelection(MenuLoginFocusedField::EMAIL, this->loginEmailSelectionState.caretByte);
    this->collapseLoginFieldSelection(MenuLoginFocusedField::PASSWORD, this->loginPasswordSelectionState.caretByte);
}

bool MenuScene::hasLoginFieldSelection(MenuLoginFocusedField field) const
{
    const MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
    return selectionState != nullptr && selectionState->anchorByte != selectionState->caretByte;
}

void MenuScene::selectAllLoginFieldText(MenuLoginFocusedField field)
{
    std::string* rawText = this->getLoginFieldValue(field);
    MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
    if (rawText == nullptr || selectionState == nullptr)
    {
        return;
    }

    selectionState->anchorByte = 0U;
    selectionState->caretByte = rawText->size();
    selectionState->dragging = false;
}

bool MenuScene::deleteSelectedLoginFieldText(MenuLoginFocusedField field)
{
    std::string* rawText = this->getLoginFieldValue(field);
    MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
    if (rawText == nullptr || selectionState == nullptr || !this->hasLoginFieldSelection(field))
    {
        return false;
    }

    const std::size_t selectionStart = (std::min)(selectionState->anchorByte, selectionState->caretByte);
    const std::size_t selectionEnd = (std::max)(selectionState->anchorByte, selectionState->caretByte);
    TextInputEditHistory* editHistory =
        (field == MenuLoginFocusedField::EMAIL)
            ? &this->loginEmailEditHistory
            : &this->loginPasswordEditHistory;
    editHistory->rememberState(*rawText, selectionState->caretByte, selectionState->anchorByte);
    rawText->erase(selectionStart, selectionEnd - selectionStart);
    this->collapseLoginFieldSelection(field, selectionStart);
    return true;
}

bool MenuScene::insertTextIntoLoginField(MenuLoginFocusedField field, const char* text)
{
    std::string* rawText = this->getLoginFieldValue(field);
    MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
    if (rawText == nullptr || selectionState == nullptr)
    {
        return false;
    }

    const std::string sanitized = menu_scene::sanitizeTextInput(text);
    if (sanitized.empty())
    {
        return false;
    }

    const std::size_t selectionStart = (std::min)(selectionState->anchorByte, selectionState->caretByte);
    const std::size_t selectionEnd = (std::max)(selectionState->anchorByte, selectionState->caretByte);
    const std::size_t replacedByteCount = selectionEnd - selectionStart;
    const std::size_t nextSize = rawText->size() - replacedByteCount + sanitized.size();
    const std::size_t maxBytes =
        (field == MenuLoginFocusedField::EMAIL)
            ? kMaxLoginEmailLength
            : kMaxLoginPasswordLength;
    if (nextSize > maxBytes)
    {
        return false;
    }

    TextInputEditHistory* editHistory =
        (field == MenuLoginFocusedField::EMAIL)
            ? &this->loginEmailEditHistory
            : &this->loginPasswordEditHistory;
    editHistory->rememberState(*rawText, selectionState->caretByte, selectionState->anchorByte);

    if (this->hasLoginFieldSelection(field))
    {
        rawText->erase(selectionStart, replacedByteCount);
        selectionState->anchorByte = selectionStart;
        selectionState->caretByte = selectionStart;
    }

    rawText->insert(selectionState->caretByte, sanitized);
    this->collapseLoginFieldSelection(field, selectionState->caretByte + sanitized.size());
    return true;
}

void MenuScene::placeLoginCaretFromMouse(MenuLoginFocusedField field, float mouseX, bool keepAnchor)
{
    MenuLoginFieldTextView textView{};
    MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
    if (selectionState == nullptr || !this->buildLoginFieldTextView(field, &textView))
    {
        return;
    }

    const float displayWidth = menu_scene::measureTextWidth(&this->menuBodyFont, textView.displayText);
    const float clampedMouseX = std::clamp(mouseX, textView.textStartX, textView.textStartX + displayWidth);
    const std::size_t displayIndex = menu_scene::findDisplayCaretIndexForX(
        &this->menuBodyFont,
        textView.displayText,
        textView.displayOffsets,
        textView.textStartX,
        clampedMouseX);
    const std::size_t clampedDisplayIndex = (std::min)(displayIndex, textView.rawOffsets.size() - 1U);
    const std::size_t nextCaretByte = textView.rawOffsets[clampedDisplayIndex];

    if (!keepAnchor)
    {
        selectionState->anchorByte = nextCaretByte;
    }
    selectionState->caretByte = nextCaretByte;
}

void MenuScene::updateLoginTextSelectionDrag(void)
{
    if (this->authFeedbackPopupVisible ||
        this->authUiSnapshot.serverSelectionVisible ||
        this->authUiSnapshot.signInRequestPending)
    {
        this->stopLoginTextSelectionDrag();
        return;
    }

    const auto updateDragForField =
        [this](MenuLoginFocusedField field)
        {
            MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
            if (selectionState == nullptr || !selectionState->dragging)
            {
                return;
            }

            if (this->focusedLoginField != field || !rc2d_mouse_isDown(RC2D_MOUSE_BUTTON_LEFT))
            {
                selectionState->dragging = false;
                return;
            }

            float mouseX = 0.0f;
            float mouseY = 0.0f;
            menu_scene::getMouseRenderPosition(&mouseX, &mouseY);
            (void)mouseY;

            this->placeLoginCaretFromMouse(field, mouseX, true);
        };

    updateDragForField(MenuLoginFocusedField::EMAIL);
    updateDragForField(MenuLoginFocusedField::PASSWORD);
}

bool MenuScene::handleLoginInputKey(
    const char* key,
    SDL_Scancode scancode,
    SDL_Keycode keycode,
    SDL_Keymod mod,
    bool isrepeat)
{
    (void)key;

    // Une fois l'overlay serveurs ouvert, le formulaire de login n'accepte plus
    // de nouvelles saisies tant que le joueur reste sur cette etape.
    if (this->authFeedbackPopupVisible ||
        this->authUiSnapshot.serverSelectionVisible ||
        this->authUiSnapshot.signInRequestPending)
    {
        return false;
    }

    // Permettre Tab meme sans focus initial pour activer rapidement le formulaire.
    if (!isrepeat && scancode == SDL_SCANCODE_TAB)
    {
        if (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
        {
            this->focusedLoginField = MenuLoginFocusedField::PASSWORD;
        }
        else
        {
            this->focusedLoginField = MenuLoginFocusedField::EMAIL;
        }
        this->stopLoginTextSelectionDrag();
        return true;
    }

    // Si aucun champ n'est actif, ignorer le reste des touches texte.
    if (this->focusedLoginField == MenuLoginFocusedField::NONE)
    {
        return false;
    }

    // Escape retire simplement le focus du formulaire.
    if (!isrepeat && scancode == SDL_SCANCODE_ESCAPE)
    {
        this->focusedLoginField = MenuLoginFocusedField::NONE;
        this->stopLoginTextSelectionDrag();
        return true;
    }

    // Enter lance la requete HTTP de connexion.
    if (!isrepeat && (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
    {
        this->submitLoginRequest();
        return true;
    }

    if (this->focusedLoginField != MenuLoginFocusedField::NONE &&
        isTextInputShortcutPressed(TextInputShortcut::UNDO, key, scancode, keycode, mod, isrepeat))
    {
        std::string* rawText = this->getLoginFieldValue(this->focusedLoginField);
        MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(this->focusedLoginField);
        TextInputEditHistory* editHistory =
            (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
                ? &this->loginEmailEditHistory
                : &this->loginPasswordEditHistory;
        if (rawText != nullptr &&
            selectionState != nullptr &&
            editHistory->undo(rawText, &selectionState->caretByte, &selectionState->anchorByte))
        {
            if (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
            {
                this->loginEmailFieldErrorMessage.clear();
            }
            else if (this->focusedLoginField == MenuLoginFocusedField::PASSWORD)
            {
                this->loginPasswordFieldErrorMessage.clear();
            }
            this->menuLocalStatusMessage.clear();
        }
        this->stopLoginTextSelectionDrag();
        return true;
    }

    if (this->focusedLoginField != MenuLoginFocusedField::NONE &&
        isTextInputShortcutPressed(TextInputShortcut::SELECT_ALL, key, scancode, keycode, mod, isrepeat))
    {
        this->selectAllLoginFieldText(this->focusedLoginField);
        return true;
    }

    if (this->focusedLoginField != MenuLoginFocusedField::NONE &&
        isTextInputShortcutPressed(TextInputShortcut::COPY, key, scancode, keycode, mod, isrepeat))
    {
        if (this->hasLoginFieldSelection(this->focusedLoginField))
        {
            const std::string* rawText = this->getLoginFieldValue(this->focusedLoginField);
            const MenuLoginTextSelectionState* selectionState =
                this->getLoginFieldSelectionState(this->focusedLoginField);
            if (rawText != nullptr && selectionState != nullptr)
            {
                const std::size_t selectionStart =
                    (std::min)(selectionState->anchorByte, selectionState->caretByte);
                const std::size_t selectionEnd =
                    (std::max)(selectionState->anchorByte, selectionState->caretByte);
                rc2d_system_setClipboardText(
                    rawText->substr(selectionStart, selectionEnd - selectionStart).c_str());
            }
        }
        return true;
    }

    if (this->focusedLoginField != MenuLoginFocusedField::NONE &&
        isTextInputShortcutPressed(TextInputShortcut::PASTE, key, scancode, keycode, mod, isrepeat))
    {
        char* clipboardText = rc2d_system_getClipboardText();
        if (clipboardText != nullptr)
        {
            const bool accepted = this->insertTextIntoLoginField(this->focusedLoginField, clipboardText);
            rc2d_system_freeClipboardText(clipboardText);
            if (accepted)
            {
                this->clearLoginFieldErrors();
                this->menuLocalStatusMessage.clear();
            }
        }
        this->stopLoginTextSelectionDrag();
        return true;
    }

    // Backspace supprime le dernier caractere du champ actif.
    if (scancode == SDL_SCANCODE_BACKSPACE)
    {
        if (this->deleteSelectedLoginFieldText(this->focusedLoginField))
        {
            this->menuLocalStatusMessage.clear();
            this->stopLoginTextSelectionDrag();
            return true;
        }

        std::string* rawText = this->getLoginFieldValue(this->focusedLoginField);
        MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(this->focusedLoginField);
        if (rawText != nullptr && selectionState != nullptr && selectionState->caretByte > 0U)
        {
            TextInputEditHistory* editHistory =
                (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
                    ? &this->loginEmailEditHistory
                    : &this->loginPasswordEditHistory;
            editHistory->rememberState(*rawText, selectionState->caretByte, selectionState->anchorByte);
            const std::vector<std::size_t> offsets = menu_scene::buildUtf8CodepointOffsets(*rawText);
            const std::size_t caretIndex =
                menu_scene::findCodepointIndexForByteOffset(offsets, selectionState->caretByte);
            if (caretIndex > 0U)
            {
                const std::size_t eraseStart = offsets[caretIndex - 1U];
                rawText->erase(eraseStart, selectionState->caretByte - eraseStart);
                this->collapseLoginFieldSelection(this->focusedLoginField, eraseStart);
            }

            if (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
            {
                this->loginEmailFieldErrorMessage.clear();
            }
            else if (this->focusedLoginField == MenuLoginFocusedField::PASSWORD)
            {
                this->loginPasswordFieldErrorMessage.clear();
            }
            this->menuLocalStatusMessage.clear();
            return true;
        }

        return true;
    }

    // Delete vide entierement le champ actuellement edite.
    if (!isrepeat && scancode == SDL_SCANCODE_DELETE)
    {
        if (this->deleteSelectedLoginFieldText(this->focusedLoginField))
        {
            if (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
            {
                this->loginEmailFieldErrorMessage.clear();
            }
            else if (this->focusedLoginField == MenuLoginFocusedField::PASSWORD)
            {
                this->loginPasswordFieldErrorMessage.clear();
            }
        }
        else
        {
            std::string* rawText = this->getLoginFieldValue(this->focusedLoginField);
            MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(this->focusedLoginField);
            if (rawText != nullptr && selectionState != nullptr)
            {
                TextInputEditHistory* editHistory =
                    (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
                        ? &this->loginEmailEditHistory
                        : &this->loginPasswordEditHistory;
                editHistory->rememberState(*rawText, selectionState->caretByte, selectionState->anchorByte);
                const std::vector<std::size_t> offsets = menu_scene::buildUtf8CodepointOffsets(*rawText);
                const std::size_t caretIndex =
                    menu_scene::findCodepointIndexForByteOffset(offsets, selectionState->caretByte);
                if (caretIndex + 1U < offsets.size())
                {
                    rawText->erase(selectionState->caretByte, offsets[caretIndex + 1U] - selectionState->caretByte);
                    this->collapseLoginFieldSelection(this->focusedLoginField, selectionState->caretByte);
                }
            }

            if (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
            {
                this->loginEmailFieldErrorMessage.clear();
            }
            else if (this->focusedLoginField == MenuLoginFocusedField::PASSWORD)
            {
                this->loginPasswordFieldErrorMessage.clear();
            }
        }
        this->menuLocalStatusMessage.clear();
        return true;
    }

    return false;
}

void MenuScene::clearLoginFieldErrors(void)
{
    this->loginEmailFieldErrorMessage.clear();
    this->loginPasswordFieldErrorMessage.clear();
}

void MenuScene::handleLoginTextInput(const char* text)
{
    if (this->authFeedbackPopupVisible ||
        this->focusedLoginField == MenuLoginFocusedField::NONE ||
        this->authUiSnapshot.serverSelectionVisible ||
        this->authUiSnapshot.signInRequestPending)
    {
        return;
    }

    bool accepted = false;
    if (this->focusedLoginField == MenuLoginFocusedField::EMAIL)
    {
        accepted = this->insertTextIntoLoginField(MenuLoginFocusedField::EMAIL, text);
        if (accepted)
        {
            this->loginEmailFieldErrorMessage.clear();
        }
    }
    else if (this->focusedLoginField == MenuLoginFocusedField::PASSWORD)
    {
        accepted = this->insertTextIntoLoginField(MenuLoginFocusedField::PASSWORD, text);
        if (accepted)
        {
            this->loginPasswordFieldErrorMessage.clear();
        }
    }

    if (accepted)
    {
        this->menuLocalStatusMessage.clear();
    }
}

void MenuScene::submitLoginRequest(void)
{
    // Si une requete est deja en vol, eviter de saturer la queue HTTP.
    if (this->authUiSnapshot.signInRequestPending)
    {
        this->menuLocalStatusMessage = "Connexion deja en cours...";
        this->menuLocalStatusIsError = false;
        return;
    }

    // Si l'overlay serveur est deja visible, rester sur l'etape de selection.
    if (this->authUiSnapshot.serverSelectionVisible)
    {
        this->menuLocalStatusMessage = "Choisissez d'abord un serveur dans la liste.";
        this->menuLocalStatusIsError = false;
        return;
    }

    // Nettoyer l'e-mail en debut/fin pour eviter les erreurs de saisie triviales.
    const std::string trimmedEmail = menu_scene::trimCopy(this->loginEmailValue);
    this->clearLoginFieldErrors();

    if (trimmedEmail.empty())
    {
        this->loginEmailFieldErrorMessage = "Identifiant / e-mail requis.";
        this->focusedLoginField = MenuLoginFocusedField::EMAIL;
        this->menuLocalStatusMessage.clear();
        this->menuLocalStatusIsError = true;
        if (this->loginPasswordValue.empty())
        {
            this->loginPasswordFieldErrorMessage = "Mot de passe requis.";
        }
        return;
    }

    if (this->loginPasswordValue.empty())
    {
        this->loginPasswordFieldErrorMessage = "Mot de passe requis.";
        this->focusedLoginField = MenuLoginFocusedField::PASSWORD;
        this->menuLocalStatusMessage.clear();
        this->menuLocalStatusIsError = true;
        return;
    }

    // Normaliser la valeur visible du champ e-mail avec la version trimmee.
    this->loginEmailValue = trimmedEmail;
    this->clearAllLoginTextSelections();
    this->stopLoginTextSelectionDrag();

    // Reinitialiser l'etat partage avant d'envoyer la nouvelle requete HTTP.
    NetworkState& networkState = GetNetworkState();
    {
        std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);

        networkState.authSignInRequestPending = true;
        networkState.authSignInLastRequestSucceeded = false;
        networkState.authSignInServerSelectionVisible = false;
        networkState.authSignInLastHttpStatusCode = 0;
        networkState.authSignInLastCode.clear();
        networkState.authSignInLastMessage.clear();
        networkState.authSignInAvailableServers.clear();
        networkState.authToken.clear();
        networkState.authTokenValidated = false;
        networkState.quilkinDns.clear();
        networkState.quilkinPort = 0;
    }

    // Refleter immediatement l'attente HTTP dans le snapshot local.
    this->authUiSnapshot.signInRequestPending = true;
    this->authUiSnapshot.signInLastRequestSucceeded = false;
    this->authUiSnapshot.serverSelectionVisible = false;
    this->authUiSnapshot.lastHttpStatusCode = 0;
    this->authUiSnapshot.lastCode.clear();
    this->authUiSnapshot.lastMessage.clear();
    this->authUiSnapshot.servers.clear();
    this->authFeedbackPopupVisible = true;
    this->hoveredAuthFeedbackPopupActionButton = false;
    this->authFeedbackPopupActionButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};

    // Effacer les messages precedents avant la nouvelle tentative.
    this->menuLocalStatusMessage = "Connexion au backend SeaTyrants en cours...";
    this->menuLocalStatusIsError = false;

    // Construire le job HTTP consomme par le worker dedie.
    SimulationToHttpMessage message{};
    message.type = SimulationToHttpMessageType::AUTH_SIGNIN_REQUEST;
    message.authSignInRequest.email = this->loginEmailValue;
    message.authSignInRequest.password = this->loginPasswordValue;

    // Envoyer la requete dans la queue Simulation -> HTTP.
    GetSimulationToHttpQueue().push(message);

    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [MENU] [AUTH_SIGNIN] - Sign-in request queued for email=%s",
        this->loginEmailValue.c_str());
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

    if (this->authFeedbackPopupVisible)
    {
        this->hoveredLanguageButton = false;
        this->hoveredLanguageScrollbar = false;
        this->hoveredLanguageFlagIndex = -1;
        this->hoveredServerSelectionButtonIndex = -1;
        this->hoveredAuthFeedbackPopupActionButton =
            !this->authUiSnapshot.signInRequestPending &&
            menu_scene::pointInRect(this->authFeedbackPopupActionButtonRect, mouseX, mouseY);
        menu_scene::applyMenuCursor(
            this->hoveredAuthFeedbackPopupActionButton
                ? SDL_SYSTEM_CURSOR_POINTER
                : SDL_SYSTEM_CURSOR_DEFAULT);
        return;
    }

    if (this->authUiSnapshot.signInRequestPending)
    {
        this->hoveredLanguageButton = false;
        this->hoveredLanguageScrollbar = false;
        this->hoveredLanguageFlagIndex = -1;
        this->hoveredServerSelectionButtonIndex = -1;
        menu_scene::applyMenuCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        return;
    }

    // Quand l'overlay serveur est visible, il devient la priorite interactive
    // absolue et bloque les interactions avec le formulaire dessous.
    this->hoveredServerSelectionButtonIndex = -1;
    if (this->authUiSnapshot.serverSelectionVisible)
    {
        this->hoveredLanguageButton = false;
        this->hoveredLanguageScrollbar = false;
        this->hoveredLanguageFlagIndex = -1;

        for (std::size_t index = 0U; index < this->serverSelectionButtonRects.size(); ++index)
        {
            if (!menu_scene::pointInRect(this->serverSelectionButtonRects[index], mouseX, mouseY))
            {
                continue;
            }

            this->hoveredServerSelectionButtonIndex = static_cast<int>(index);
            break;
        }

        menu_scene::applyMenuCursor(
            this->hoveredServerSelectionButtonIndex >= 0
                ? SDL_SYSTEM_CURSOR_POINTER
                : SDL_SYSTEM_CURSOR_DEFAULT);
        return;
    }

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

    const bool hoveringEmail = rc2d_collision_pointInUIImagePixelPerfect(&this->inputEmailUi, mouseX, mouseY);
    const bool hoveringPassword = rc2d_collision_pointInUIImagePixelPerfect(&this->inputPasswordUi, mouseX, mouseY);
    const bool hoveringLogin = menu_scene::pointInRect(this->buttonLoginUi.last_drawn_rect, mouseX, mouseY);
    if (hoveringEmail || hoveringPassword)
    {
        menu_scene::applyMenuCursor(SDL_SYSTEM_CURSOR_TEXT);
        return;
    }

    const bool wantsPointer =
        this->hoveredLanguageButton ||
        this->hoveredLanguageScrollbar ||
        (this->hoveredLanguageFlagIndex >= 0) ||
        hoveringLogin;

    menu_scene::applyMenuCursor(wantsPointer ? SDL_SYSTEM_CURSOR_POINTER : SDL_SYSTEM_CURSOR_DEFAULT);
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

    // Relacher les polices utilisees par le formulaire et l'overlay serveur.
    ResetStorageFontRef(&this->menuTitleFont);
    ResetStorageFontRef(&this->menuBodyFont);

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
    this->serverSelectionOverlayRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionHeaderRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionRowRects.clear();
    this->serverSelectionButtonRects.clear();
    this->hoveredLanguageButton = false;
    this->hoveredLanguageScrollbar = false;
    this->hoveredLanguageFlagIndex = -1;
    this->selectedLanguageFlagIndex = -1;
    this->hoveredServerSelectionButtonIndex = -1;
    this->languageDropdownOpen = false;
    this->languageScrollDragging = false;
    this->languageScrollWheelHighlightSec = 0.0f;
    this->languageScrollOffset = 0.0f;
    this->maxLanguageScrollOffset = 0.0f;
    this->languageScrollDragOffsetY = 0.0f;
    this->loginEmailValue.clear();
    this->loginPasswordValue.clear();
    this->loginEmailFieldErrorMessage.clear();
    this->loginPasswordFieldErrorMessage.clear();
    this->focusedLoginField = MenuLoginFocusedField::NONE;
    this->menuLocalStatusMessage.clear();
    this->menuLocalStatusIsError = false;
    this->authUiSnapshot = MenuAuthUiSnapshot{};
    this->authFeedbackPopupVisible = false;
    this->loginEmailSelectionState = MenuLoginTextSelectionState{};
    this->loginPasswordSelectionState = MenuLoginTextSelectionState{};
    this->loginEmailEditHistory.clear();
    this->loginPasswordEditHistory.clear();
    this->authFeedbackPopupRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->authFeedbackPopupActionButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->hoveredAuthFeedbackPopupActionButton = false;

    // Restore a neutral cursor when leaving the menu.
    menu_scene::applyMenuCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    rc2d_keyboard_setTextInput(false);

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
    menu_scene::applyMenuCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    rc2d_keyboard_setTextInput(true);

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
    this->languageScrollWheelHighlightSec = 0.0f;
    this->languageScrollOffset = 0.0f;
    this->maxLanguageScrollOffset = 0.0f;
    this->languageScrollDragOffsetY = 0.0f;
    this->languageButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageDropdownRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageListViewportRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollTrackRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->languageScrollThumbRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->loginCardRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionOverlayRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionHeaderRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->serverSelectionRowRects.clear();
    this->serverSelectionButtonRects.clear();
    this->hoveredServerSelectionButtonIndex = -1;
    this->focusedLoginField = MenuLoginFocusedField::EMAIL;
    this->loginEmailFieldErrorMessage.clear();
    this->loginPasswordFieldErrorMessage.clear();
    this->menuLocalStatusMessage.clear();
    this->menuLocalStatusIsError = false;
    this->authUiSnapshot = MenuAuthUiSnapshot{};
    this->authFeedbackPopupVisible = false;
    this->loginEmailSelectionState = MenuLoginTextSelectionState{};
    this->loginPasswordSelectionState = MenuLoginTextSelectionState{};
    this->loginEmailEditHistory.clear();
    this->loginPasswordEditHistory.clear();
    this->authFeedbackPopupRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->authFeedbackPopupActionButtonRect = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    this->hoveredAuthFeedbackPopupActionButton = false;

    // Charger les polices utilisees pour le texte saisi et l'overlay serveur.
    this->menuTitleFont = OpenStorageFont("assets/fonts/TradeWinds-Regular.ttf", RC2D_STORAGE_TITLE, 24.0f);
    this->menuBodyFont = OpenStorageFont("assets/fonts/SegoeUI-Regular.ttf", RC2D_STORAGE_TITLE, 18.0f);

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
    this->refreshAuthUiSnapshotFromNetworkState();
    this->updateLanguageFlagLayout();
    this->snapLanguageScrollToSelection();
    this->rebuildServerSelectionLayout();

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
    this->refreshAuthUiSnapshotFromNetworkState();
    this->rebuildLanguageSelectionFromContext();
    this->updateLanguageFlagLayout();
    this->updateLanguageScrollbarDrag();
    this->updateLanguageFlagLayout();
    if (this->authUiSnapshot.signInRequestPending)
    {
        this->authFeedbackPopupVisible = true;
    }
    else if (this->authUiSnapshot.serverSelectionVisible || this->authUiSnapshot.signInLastRequestSucceeded)
    {
        this->authFeedbackPopupVisible = false;
    }
    else if (this->authFeedbackPopupVisible &&
             this->authUiSnapshot.lastMessage.empty() &&
             this->authUiSnapshot.lastCode.empty() &&
             this->authUiSnapshot.lastHttpStatusCode == 0)
    {
        this->authFeedbackPopupVisible = false;
    }
    this->rebuildAuthFeedbackOverlayLayout();
    this->rebuildServerSelectionLayout();
    this->updateLoginTextSelectionDrag();

    // Laisser ensuite les messages backend reprendre la main une fois que la
    // requete HTTP n'est plus en cours.
    if (!this->authUiSnapshot.signInRequestPending &&
        this->menuLocalStatusMessage == "Connexion au backend SeaTyrants en cours...")
    {
        this->menuLocalStatusMessage.clear();
    }

    this->updateMenuInteractivity();

    if (this->languageScrollWheelHighlightSec > 0.0f)
    {
        this->languageScrollWheelHighlightSec -= static_cast<float>(dt);
        if (this->languageScrollWheelHighlightSec < 0.0f)
        {
            this->languageScrollWheelHighlightSec = 0.0f;
        }
    }
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
            (this->languageScrollDragging || (this->languageScrollWheelHighlightSec > 0.0f))
                ? RC2D_Color{184, 132, 30, 245}
                : (this->hoveredLanguageScrollbar ? menu_scene::kFlagHoverBorder : menu_scene::kFlagSelectedBorder);
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

void MenuScene::drawLoginFieldContents(void)
{
    // Sans police chargee, il est impossible d'afficher le texte saisi.
    if (this->menuBodyFont.sdl_font == nullptr)
    {
        return;
    }

    // Le curseur clignotant reste volontairement simple pour garder l'UI lisible.
    const bool blinkVisible = std::fmod(this->ambientAnimationTime, 0.9) < 0.45;

    const auto drawSingleField =
        [this, blinkVisible](
            MenuLoginFocusedField field,
            bool focused)
        {
            MenuLoginFieldTextView textView{};
            if (!this->buildLoginFieldTextView(field, &textView))
            {
                return;
            }

            const MenuLoginTextSelectionState* selectionState = this->getLoginFieldSelectionState(field);
            if (focused && selectionState != nullptr && selectionState->anchorByte != selectionState->caretByte)
            {
                const std::size_t selectionStartByte = (std::min)(selectionState->anchorByte, selectionState->caretByte);
                const std::size_t selectionEndByte = (std::max)(selectionState->anchorByte, selectionState->caretByte);
                const std::size_t selectionStartIndex =
                    menu_scene::findCodepointIndexForByteOffset(textView.rawOffsets, selectionStartByte);
                const std::size_t selectionEndIndex =
                    menu_scene::findCodepointIndexForByteOffset(textView.rawOffsets, selectionEndByte);

                const float prefixWidth = menu_scene::measureTextWidth(
                    &this->menuBodyFont,
                    textView.displayText.substr(0U, textView.displayOffsets[selectionStartIndex]));
                const float selectionWidth = menu_scene::measureTextWidth(
                    &this->menuBodyFont,
                    textView.displayText.substr(
                        textView.displayOffsets[selectionStartIndex],
                        textView.displayOffsets[selectionEndIndex] - textView.displayOffsets[selectionStartIndex]));
                SDL_FRect selectionRect = SDL_FRect{
                    std::round(textView.textStartX + prefixWidth - 1.0f),
                    std::round(textView.textRect.y + 3.0f),
                    std::round(selectionWidth + 2.0f),
                    std::round(textView.textRect.h - 6.0f)};
                if (selectionRect.w > 0.0f)
                {
                    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
                    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kFlagSelectedFill, 0.88f));
                    rc2d_graphics_rectangle("fill", &selectionRect);
                    rc2d_graphics_setColor(menu_scene::applyAlpha(menu_scene::kFlagSelectedBorder, 0.92f));
                    rc2d_graphics_rectangle("line", &selectionRect);
                    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);
                }
            }

            menu_scene::drawTextLeftCenteredY(
                &this->menuBodyFont,
                textView.displayText,
                textView.textRect,
                textView.textStartX,
                menu_scene::kBodyText);

            if (focused && blinkVisible && selectionState != nullptr && selectionState->anchorByte == selectionState->caretByte)
            {
                const std::size_t caretIndex =
                    menu_scene::findCodepointIndexForByteOffset(textView.rawOffsets, selectionState->caretByte);
                const float cursorX = std::round(
                    textView.textStartX +
                    menu_scene::measureTextWidth(
                        &this->menuBodyFont,
                        textView.displayText.substr(0U, textView.displayOffsets[caretIndex])));
                const float cursorTop = std::round(textView.textRect.y + 4.0f);
                const float cursorBottom = std::round(textView.textRect.y + textView.textRect.h - 4.0f);
                rc2d_graphics_setColor(menu_scene::kBodyText);
                rc2d_graphics_line(cursorX, cursorTop, cursorX, cursorBottom);
            }
        };

    // Dessiner le contenu des deux inputs exactement a l'endroit de leurs images.
    drawSingleField(
        MenuLoginFocusedField::EMAIL,
        this->focusedLoginField == MenuLoginFocusedField::EMAIL);
    drawSingleField(
        MenuLoginFocusedField::PASSWORD,
        this->focusedLoginField == MenuLoginFocusedField::PASSWORD);
}

void MenuScene::drawLoginStatusMessage(void)
{
    // Sans police, ne rien tenter pour eviter un rendu incoherent.
    if (this->menuBodyFont.sdl_font == nullptr || !menu_scene::isValidRect(this->inputPasswordUi.last_drawn_rect))
    {
        return;
    }

    // Priorite:
    // 1. message local de validation / action temporaire
    // 2. message "connexion en cours"
    // 3. erreur backend
    // 4. succes backend avant selection serveur
    std::string messageToDraw;
    RC2D_Color messageColor = menu_scene::kMutedText;

    if (!this->menuLocalStatusMessage.empty())
    {
        messageToDraw = this->menuLocalStatusMessage;
        messageColor = this->menuLocalStatusIsError ? menu_scene::kErrorText : menu_scene::kMutedText;
    }
    else if (!this->authUiSnapshot.signInLastRequestSucceeded &&
             (!this->authUiSnapshot.lastMessage.empty() ||
              !this->authUiSnapshot.lastCode.empty() ||
              this->authUiSnapshot.lastHttpStatusCode != 0))
    {
        messageToDraw = this->authUiSnapshot.lastMessage;
        if (messageToDraw.empty())
        {
            messageToDraw = "Erreur de connexion";
        }
        if (this->authUiSnapshot.lastHttpStatusCode != 0)
        {
            messageToDraw += " (HTTP " + std::to_string(this->authUiSnapshot.lastHttpStatusCode) + ")";
        }
        messageColor = menu_scene::kErrorText;
    }
    else if (this->authUiSnapshot.serverSelectionVisible)
    {
        messageToDraw = "Connexion web reussie. Selectionnez maintenant un serveur.";
        messageColor = menu_scene::kSuccessText;
    }

    if (messageToDraw.empty())
    {
        return;
    }

    const SDL_FRect statusRect = SDL_FRect{
        this->inputPasswordUi.last_drawn_rect.x - 20.0f,
        this->inputPasswordUi.last_drawn_rect.y + this->inputPasswordUi.last_drawn_rect.h + 32.0f,
        this->inputPasswordUi.last_drawn_rect.w + 40.0f,
        32.0f};
    menu_scene::drawCenteredText(&this->menuBodyFont, messageToDraw, statusRect, messageColor);
}

void MenuScene::drawLoginFieldErrors(void)
{
    if (this->menuBodyFont.sdl_font == nullptr)
    {
        return;
    }

    if (!this->loginEmailFieldErrorMessage.empty() && menu_scene::isValidRect(this->inputEmailUi.last_drawn_rect))
    {
        const SDL_FRect errorRect = SDL_FRect{
            this->inputEmailUi.last_drawn_rect.x + 26.0f,
            this->inputEmailUi.last_drawn_rect.y + this->inputEmailUi.last_drawn_rect.h - 3.0f,
            this->inputEmailUi.last_drawn_rect.w - 52.0f,
            24.0f};
        menu_scene::drawTextLeftCenteredY(
            &this->menuBodyFont,
            this->loginEmailFieldErrorMessage,
            errorRect,
            errorRect.x,
            menu_scene::kErrorText);
    }

    if (!this->loginPasswordFieldErrorMessage.empty() && menu_scene::isValidRect(this->inputPasswordUi.last_drawn_rect))
    {
        const SDL_FRect errorRect = SDL_FRect{
            this->inputPasswordUi.last_drawn_rect.x + 26.0f,
            this->inputPasswordUi.last_drawn_rect.y + this->inputPasswordUi.last_drawn_rect.h + 4.0f,
            this->inputPasswordUi.last_drawn_rect.w - 52.0f,
            24.0f};
        menu_scene::drawTextLeftCenteredY(
            &this->menuBodyFont,
            this->loginPasswordFieldErrorMessage,
            errorRect,
            errorRect.x,
            menu_scene::kErrorText);
    }
}

void MenuScene::drawAuthFeedbackOverlay(void)
{
    if (!this->authFeedbackPopupVisible || !menu_scene::isValidRect(this->authFeedbackPopupRect))
    {
        return;
    }

    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    const SDL_FRect panelRect = this->authFeedbackPopupRect;
    const SDL_FRect innerRect = menu_scene::expandRect(panelRect, -8.0f, -8.0f);
    const bool isPending = this->authUiSnapshot.signInRequestPending;

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(menu_scene::kOverlayDim);
    rc2d_graphics_rectangle("fill", &safeRect);
    rc2d_graphics_setColor(menu_scene::kPendingPanelFill);
    rc2d_graphics_rectangle("fill", &panelRect);
    rc2d_graphics_setColor(menu_scene::kPendingPanelBorder);
    rc2d_graphics_rectangle("line", &panelRect);
    rc2d_graphics_setColor(menu_scene::kCardInnerBorder);
    rc2d_graphics_rectangle("line", &innerRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    const float centerX = panelRect.x + (panelRect.w * 0.5f);
    if (isPending)
    {
        const float centerY = panelRect.y + 62.0f;
        const float radius = 18.0f;
        for (int spokeIndex = 0; spokeIndex < 10; ++spokeIndex)
        {
            const float angle =
                static_cast<float>(this->ambientAnimationTime * 4.5) +
                (static_cast<float>(spokeIndex) * 0.62831853f);
            RC2D_Color spokeColor = menu_scene::applyAlpha(
                menu_scene::kPendingLoaderColor,
                0.25f + (static_cast<float>(spokeIndex) / 12.0f));
            rc2d_graphics_setColor(spokeColor);
            rc2d_graphics_line(
                centerX + (std::cos(angle) * (radius - 6.0f)),
                centerY + (std::sin(angle) * (radius - 6.0f)),
                centerX + (std::cos(angle) * radius),
                centerY + (std::sin(angle) * radius));
        }

        const SDL_FRect titleRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 84.0f, panelRect.w - 44.0f, 30.0f};
        const SDL_FRect bodyRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 118.0f, panelRect.w - 44.0f, 36.0f};
        const SDL_FRect detailRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 150.0f, panelRect.w - 44.0f, 24.0f};
        if (this->menuTitleFont.sdl_font != nullptr)
        {
            menu_scene::drawCenteredText(
                &this->menuTitleFont,
                "Connexion en cours",
                titleRect,
                menu_scene::kTitleText);
        }
        menu_scene::drawCenteredText(
            &this->menuBodyFont,
            "Tentative de connexion au backend SeaTyrants...",
            bodyRect,
            menu_scene::kBodyText);
#if GAME_ENV_DEV
        menu_scene::drawCenteredText(
            &this->menuBodyFont,
            "Mode DEV: connexion HTTP locale.",
            detailRect,
            menu_scene::kMutedText);
#else
        menu_scene::drawCenteredText(
            &this->menuBodyFont,
            "Attente de la reponse du service d'authentification.",
            detailRect,
            menu_scene::kMutedText);
#endif
        return;
    }

    const float iconCenterY = panelRect.y + 44.0f;
    rc2d_graphics_setColor(menu_scene::kErrorText);
    rc2d_graphics_line(centerX - 12.0f, iconCenterY - 12.0f, centerX + 12.0f, iconCenterY + 12.0f);
    rc2d_graphics_line(centerX - 12.0f, iconCenterY + 12.0f, centerX + 12.0f, iconCenterY - 12.0f);

    const std::string titleText =
        menu_scene::buildAuthFeedbackFailureTitle(this->authUiSnapshot.lastHttpStatusCode, this->authUiSnapshot.lastCode);
    const std::string bodyText =
        !this->authUiSnapshot.lastMessage.empty()
            ? this->authUiSnapshot.lastMessage
            : std::string("La connexion au backend SeaTyrants a echoue.");
    const std::string detailText =
        menu_scene::buildAuthFeedbackDetailText(this->authUiSnapshot.lastHttpStatusCode, this->authUiSnapshot.lastCode);
    const std::string hintText =
        menu_scene::buildAuthFeedbackHintText(this->authUiSnapshot.lastHttpStatusCode, this->authUiSnapshot.lastCode);

    const SDL_FRect titleRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 66.0f, panelRect.w - 44.0f, 32.0f};
    const SDL_FRect bodyRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 108.0f, panelRect.w - 44.0f, 34.0f};
    const SDL_FRect detailRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 146.0f, panelRect.w - 44.0f, 26.0f};
    const SDL_FRect hintRect = SDL_FRect{panelRect.x + 22.0f, panelRect.y + 178.0f, panelRect.w - 44.0f, 40.0f};
    if (this->menuTitleFont.sdl_font != nullptr)
    {
        menu_scene::drawCenteredText(
            &this->menuTitleFont,
            titleText,
            titleRect,
            menu_scene::kTitleText);
    }
    menu_scene::drawCenteredText(&this->menuBodyFont, bodyText, bodyRect, menu_scene::kBodyText);
    menu_scene::drawCenteredText(&this->menuBodyFont, detailText, detailRect, menu_scene::kMutedText);
    menu_scene::drawCenteredText(&this->menuBodyFont, hintText, hintRect, menu_scene::kMutedText);

    if (menu_scene::isValidRect(this->authFeedbackPopupActionButtonRect))
    {
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(
            this->hoveredAuthFeedbackPopupActionButton
                ? menu_scene::kServerButtonHoverFill
                : menu_scene::kServerButtonFill);
        rc2d_graphics_rectangle("fill", &this->authFeedbackPopupActionButtonRect);
        rc2d_graphics_setColor(menu_scene::kServerButtonBorder);
        rc2d_graphics_rectangle("line", &this->authFeedbackPopupActionButtonRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        menu_scene::drawCenteredText(
            &this->menuBodyFont,
            "Fermer",
            this->authFeedbackPopupActionButtonRect,
            menu_scene::kTitleText);
    }
}

void MenuScene::drawServerSelectionOverlay(void)
{
    // Sortir si l'etape de selection serveur n'est pas active.
    if (!this->authUiSnapshot.serverSelectionVisible || !menu_scene::isValidRect(this->serverSelectionOverlayRect))
    {
        return;
    }

    // Assombrir le menu derriere l'overlay pour attirer le regard sur la selection.
    const SDL_FRect safeRect = rc2d_engine_getVisibleSafeRectRender();
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(menu_scene::kOverlayDim);
    rc2d_graphics_rectangle("fill", &safeRect);

    // Dessiner le panneau principal avec le meme langage visuel que le reste du menu.
    SDL_FRect shadowRect = this->serverSelectionOverlayRect;
    shadowRect.x += 12.0f;
    shadowRect.y += 14.0f;
    const SDL_FRect innerRect = menu_scene::expandRect(this->serverSelectionOverlayRect, -8.0f, -8.0f);

    rc2d_graphics_setColor(menu_scene::kCardShadow);
    rc2d_graphics_rectangle("fill", &shadowRect);
    rc2d_graphics_setColor(menu_scene::kCardFill);
    rc2d_graphics_rectangle("fill", &this->serverSelectionOverlayRect);
    rc2d_graphics_setColor(menu_scene::kCardBorder);
    rc2d_graphics_rectangle("line", &this->serverSelectionOverlayRect);
    rc2d_graphics_setColor(menu_scene::kCardInnerFill);
    rc2d_graphics_rectangle("fill", &innerRect);
    rc2d_graphics_setColor(menu_scene::kCardInnerBorder);
    rc2d_graphics_rectangle("line", &innerRect);
    rc2d_graphics_setColor(menu_scene::kHeaderFill);
    rc2d_graphics_rectangle("fill", &this->serverSelectionHeaderRect);
    rc2d_graphics_setColor(menu_scene::kHeaderLine);
    rc2d_graphics_rectangle("line", &this->serverSelectionHeaderRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    // Afficher le titre principal.
    if (this->menuTitleFont.sdl_font != nullptr)
    {
        menu_scene::drawCenteredText(
            &this->menuTitleFont,
            "Liste des serveurs",
            this->serverSelectionHeaderRect,
            menu_scene::kTitleText);
    }

    const SDL_FRect tableHeaderRect = SDL_FRect{
        this->serverSelectionOverlayRect.x + 18.0f,
        this->serverSelectionHeaderRect.y + this->serverSelectionHeaderRect.h + 12.0f,
        this->serverSelectionOverlayRect.w - 36.0f,
        28.0f};
    const SDL_FRect nameHeaderRect = SDL_FRect{
        tableHeaderRect.x + 16.0f,
        tableHeaderRect.y,
        tableHeaderRect.w * 0.42f,
        tableHeaderRect.h};
    const SDL_FRect regionHeaderRect = SDL_FRect{
        tableHeaderRect.x + (tableHeaderRect.w * 0.46f),
        tableHeaderRect.y,
        120.0f,
        tableHeaderRect.h};
    const SDL_FRect statusHeaderRect = SDL_FRect{
        tableHeaderRect.x + (tableHeaderRect.w * 0.62f),
        tableHeaderRect.y,
        110.0f,
        tableHeaderRect.h};
    const SDL_FRect actionHeaderRect = SDL_FRect{
        std::round(tableHeaderRect.x + tableHeaderRect.w - 150.0f),
        tableHeaderRect.y,
        132.0f,
        tableHeaderRect.h};

    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
    rc2d_graphics_setColor(menu_scene::kHeaderFill);
    rc2d_graphics_rectangle("fill", &tableHeaderRect);
    rc2d_graphics_setColor(menu_scene::kCardInnerBorder);
    rc2d_graphics_rectangle("line", &tableHeaderRect);
    rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

    menu_scene::drawTextLeftCenteredY(
        &this->menuBodyFont,
        "Nom du serveur",
        nameHeaderRect,
        nameHeaderRect.x,
        menu_scene::kTitleText);
    menu_scene::drawTextLeftCenteredY(
        &this->menuBodyFont,
        "Region",
        regionHeaderRect,
        regionHeaderRect.x,
        menu_scene::kTitleText);
    menu_scene::drawTextLeftCenteredY(
        &this->menuBodyFont,
        "Statut",
        statusHeaderRect,
        statusHeaderRect.x,
        menu_scene::kTitleText);
    menu_scene::drawCenteredText(
        &this->menuBodyFont,
        "Action",
        actionHeaderRect,
        menu_scene::kTitleText);

    // Si la liste est vide, l'overlay reste visible mais affiche un message explicite.
    if (this->authUiSnapshot.servers.empty())
    {
        const SDL_FRect emptyRect = SDL_FRect{
            this->serverSelectionOverlayRect.x + 20.0f,
            tableHeaderRect.y + tableHeaderRect.h + 20.0f,
            this->serverSelectionOverlayRect.w - 40.0f,
            46.0f};
        menu_scene::drawCenteredText(
            &this->menuBodyFont,
            "Aucun serveur disponible pour le moment.",
            emptyRect,
            menu_scene::kBodyText);
        return;
    }

    // Dessiner une ligne par serveur: nom, region, etat, bouton.
    const std::size_t rowCount = (std::min)(
        this->authUiSnapshot.servers.size(),
        (std::min)(this->serverSelectionRowRects.size(), this->serverSelectionButtonRects.size()));
    for (std::size_t index = 0U; index < rowCount; ++index)
    {
        const AuthSignInHTTPServerEntry& serverEntry = this->authUiSnapshot.servers[index];
        const SDL_FRect& rowRect = this->serverSelectionRowRects[index];
        const SDL_FRect& buttonRect = this->serverSelectionButtonRects[index];

        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_BLEND);
        rc2d_graphics_setColor(menu_scene::kServerRowFill);
        rc2d_graphics_rectangle("fill", &rowRect);
        rc2d_graphics_setColor(menu_scene::kServerRowBorder);
        rc2d_graphics_rectangle("line", &rowRect);
        rc2d_graphics_setColor(
            static_cast<int>(index) == this->hoveredServerSelectionButtonIndex
                ? menu_scene::kServerButtonHoverFill
                : menu_scene::kServerButtonFill);
        rc2d_graphics_rectangle("fill", &buttonRect);
        rc2d_graphics_setColor(menu_scene::kServerButtonBorder);
        rc2d_graphics_rectangle("line", &buttonRect);
        rc2d_graphics_setBlendMode(RC2D_BLENDMODE_NONE);

        // Colonne nom.
        const SDL_FRect nameRect = SDL_FRect{
            rowRect.x + 16.0f,
            rowRect.y,
            rowRect.w * 0.42f,
            rowRect.h};
        menu_scene::drawTextLeftCenteredY(
            &this->menuBodyFont,
            serverEntry.name,
            nameRect,
            nameRect.x,
            menu_scene::kBodyText);

        // Colonne region.
        const SDL_FRect regionRect = SDL_FRect{
            rowRect.x + (rowRect.w * 0.46f),
            rowRect.y,
            120.0f,
            rowRect.h};
        menu_scene::drawTextLeftCenteredY(
            &this->menuBodyFont,
            serverEntry.region,
            regionRect,
            regionRect.x,
            menu_scene::kMutedText);

        // Colonne statut d'ouverture.
        const SDL_FRect statusRect = SDL_FRect{
            rowRect.x + (rowRect.w * 0.62f),
            rowRect.y,
            110.0f,
            rowRect.h};
        menu_scene::drawTextLeftCenteredY(
            &this->menuBodyFont,
            serverEntry.isClosed ? "Ferme" : "Ouvert",
            statusRect,
            statusRect.x,
            serverEntry.isClosed ? menu_scene::kServerClosedText : menu_scene::kServerOpenText);

        // Bouton de ligne.
        menu_scene::drawCenteredText(
            &this->menuBodyFont,
            "Connexion",
            buttonRect,
            menu_scene::kTitleText);
    }
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
    this->drawLoginFieldContents();
    this->drawLoginFieldErrors();
    this->drawLoginStatusMessage();
    this->drawServerSelectionOverlay();
    this->drawAuthFeedbackOverlay();

    // Draw intro fade if still active.
    if (this->loginFadeAlpha > 0.0f)
    {
        this->drawFullscreenBlackWithAlpha(this->loginFadeAlpha);
    }

    // Version build (CMake APP_VERSION) en bas a droite de l'aire jeu.
    if (this->menuBodyFont.sdl_font != nullptr)
    {
        const SDL_FRect screenRect = GetGameScreen().rect;
        if (menu_scene::isValidRect(screenRect))
        {
            constexpr float kVersionMargin = 10.0f;
            menu_scene::drawTextRightBottom(
                &this->menuBodyFont,
                APP_VERSION,
                screenRect,
                kVersionMargin,
                kVersionMargin,
                menu_scene::kMutedText);
        }
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
    (void)keyboardID;

    if (this->authFeedbackPopupVisible)
    {
        if (this->authUiSnapshot.signInRequestPending)
        {
            return;
        }

        if (!isrepeat &&
            (keycode == SDLK_ESCAPE ||
             keycode == SDLK_RETURN ||
             keycode == SDLK_KP_ENTER ||
             keycode == SDLK_SPACE))
        {
            this->authFeedbackPopupVisible = false;
            this->hoveredAuthFeedbackPopupActionButton = false;
            return;
        }

        return;
    }

    // Laisser le formulaire du menu consommer les touches de saisie en priorite.
    if (this->handleLoginInputKey(key, scancode, keycode, mod, isrepeat))
    {
        return;
    }

    // Lorsque l'overlay serveur est visible, ignorer les autres raccourcis du menu.
    if (this->authUiSnapshot.serverSelectionVisible || this->authUiSnapshot.signInRequestPending)
    {
        return;
    }

    // A ce stade, Enter sur le menu lance simplement la meme requete signin
    // que le bouton de connexion.
    if (!isrepeat && (keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER))
    {
        this->submitLoginRequest();
        return;
    }
}

void MenuScene::textinput(const RC2D_TextInputEventInfo* info)
{
    if (info == nullptr)
    {
        return;
    }

    if (this->authFeedbackPopupVisible)
    {
        return;
    }

    this->handleLoginTextInput(info->text);
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

    if (this->authFeedbackPopupVisible)
    {
        if (this->authUiSnapshot.signInRequestPending)
        {
            return;
        }

        this->authFeedbackPopupVisible = false;
        this->hoveredAuthFeedbackPopupActionButton = false;
        return;
    }

    if (this->authUiSnapshot.signInRequestPending)
    {
        return;
    }

    // Si l'overlay de serveurs est ouvert, il capture tout le clic gauche.
    if (this->authUiSnapshot.serverSelectionVisible)
    {
        const std::size_t rowCount = (std::min)(
            this->authUiSnapshot.servers.size(),
            this->serverSelectionButtonRects.size());
        for (std::size_t index = 0U; index < rowCount; ++index)
        {
            if (!menu_scene::pointInRect(this->serverSelectionButtonRects[index], x, y))
            {
                continue;
            }

            // A ce stade, le bouton ne connecte pas encore au serveur du jeu.
            this->menuLocalStatusMessage =
                "Connexion reseau au serveur \"" + this->authUiSnapshot.servers[index].name +
                "\" non activee pour le moment.";
            this->menuLocalStatusIsError = false;

            RC2D_log(
                RC2D_LOG_INFO,
                "[CLIENT] [MENU] [AUTH_SIGNIN] - Clicked server row connect button (server=%s, region=%s, isClosed=%d).",
                this->authUiSnapshot.servers[index].name.c_str(),
                this->authUiSnapshot.servers[index].region.c_str(),
                this->authUiSnapshot.servers[index].isClosed ? 1 : 0);
            return;
        }

        // Les clics hors boutons sont absorbes par l'overlay pour eviter de
        // reinteragir avec le menu place dessous.
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
            this->languageScrollWheelHighlightSec = 0.0f;
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
        this->languageScrollWheelHighlightSec = 0.0f;
        this->updateLanguageFlagLayout();
        this->updateMenuInteractivity();
    }

    // Check email box hit.
    if (rc2d_collision_pointInUIImagePixelPerfect(&this->inputEmailUi, x, y))
    {
        this->focusedLoginField = MenuLoginFocusedField::EMAIL;
        this->stopLoginTextSelectionDrag();
        this->collapseLoginFieldSelection(
            MenuLoginFocusedField::PASSWORD,
            this->loginPasswordSelectionState.caretByte);
        this->loginEmailFieldErrorMessage.clear();
        this->menuLocalStatusMessage.clear();
        if (clicks >= 2)
        {
            this->selectAllLoginFieldText(MenuLoginFocusedField::EMAIL);
        }
        else
        {
            this->placeLoginCaretFromMouse(MenuLoginFocusedField::EMAIL, x, false);
            this->loginEmailSelectionState.dragging = true;
        }
        RC2D_log(RC2D_LOG_INFO, "Clicked EMAIL input box");
    }
    // Check password box hit.
    else if (rc2d_collision_pointInUIImagePixelPerfect(&this->inputPasswordUi, x, y))
    {
        this->focusedLoginField = MenuLoginFocusedField::PASSWORD;
        this->stopLoginTextSelectionDrag();
        this->collapseLoginFieldSelection(
            MenuLoginFocusedField::EMAIL,
            this->loginEmailSelectionState.caretByte);
        this->loginPasswordFieldErrorMessage.clear();
        this->menuLocalStatusMessage.clear();
        if (clicks >= 2)
        {
            this->selectAllLoginFieldText(MenuLoginFocusedField::PASSWORD);
        }
        else
        {
            this->placeLoginCaretFromMouse(MenuLoginFocusedField::PASSWORD, x, false);
            this->loginPasswordSelectionState.dragging = true;
        }
        RC2D_log(RC2D_LOG_INFO, "Clicked PASSWORD input box");
    }
    // Check login button hit.
    else if (menu_scene::pointInRect(this->buttonLoginUi.last_drawn_rect, x, y))
    {
        this->stopLoginTextSelectionDrag();
        this->submitLoginRequest();
        RC2D_log(RC2D_LOG_INFO, "Clicked LOGIN button");
    }
    else
    {
        // Cliquer ailleurs retire le focus des champs pour garder un comportement simple.
        this->focusedLoginField = MenuLoginFocusedField::NONE;
        this->stopLoginTextSelectionDrag();
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

    if (this->authFeedbackPopupVisible)
    {
        return;
    }

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
        const float offsetBefore = this->languageScrollOffset;
        this->scrollLanguageDropdown(-(static_cast<float>(delta) * menu_scene::kLanguageScrollWheelStep));
        if (this->languageScrollOffset != offsetBefore)
        {
            this->languageScrollWheelHighlightSec = menu_scene::kScrollThumbWheelHighlightSec;
        }
        this->updateLanguageFlagLayout();
        this->updateMenuInteractivity();
    }
}
