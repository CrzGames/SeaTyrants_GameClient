#include "game/ui/hud/window-control-icons.h"

#include "game/assets/title-asset-cache.h"

#include <algorithm>
#include <cmath>

WindowControlIcons::WindowControlIcons(void)
    : closeIcon{},
      lockIcon{},
      unlockIcon{},
      scaleIcon{}
{
}

WindowControlIcons::~WindowControlIcons(void)
{
}

void WindowControlIcons::load(void)
{
    this->closeIcon = LoadStorageImage(
        "assets/images/ui-scene-game/icon-cross.png",
        RC2D_STORAGE_TITLE);
    this->lockIcon = LoadStorageImage(
        "assets/images/ui-scene-game/icon-lock.png",
        RC2D_STORAGE_TITLE);
    this->unlockIcon = LoadStorageImage(
        "assets/images/ui-scene-game/icon-unlock.png",
        RC2D_STORAGE_TITLE);
    this->scaleIcon = LoadStorageImage(
        "assets/images/ui-scene-game/icon-scale.png",
        RC2D_STORAGE_TITLE);
}

void WindowControlIcons::unload(void)
{
    ResetStorageImageRef(&this->scaleIcon);
    ResetStorageImageRef(&this->unlockIcon);
    ResetStorageImageRef(&this->lockIcon);
    ResetStorageImageRef(&this->closeIcon);
}

void WindowControlIcons::drawCloseButton(const SDL_FRect& buttonRect, RC2D_Color fillColor, RC2D_Color borderColor) const
{
    this->drawButtonFrame(buttonRect, fillColor, borderColor);
    this->drawCenteredIcon(this->closeIcon, buttonRect);
}

void WindowControlIcons::drawLockButton(
    const SDL_FRect& buttonRect,
    bool locked,
    RC2D_Color fillColor,
    RC2D_Color borderColor) const
{
    this->drawButtonFrame(buttonRect, fillColor, borderColor);
    this->drawCenteredIcon(locked ? this->lockIcon : this->unlockIcon, buttonRect);
}

void WindowControlIcons::drawResizeHandle(
    const SDL_FRect& buttonRect,
    RC2D_Color fillColor,
    RC2D_Color borderColor) const
{
    this->drawButtonFrame(buttonRect, fillColor, borderColor);
    this->drawCenteredIcon(this->scaleIcon, buttonRect, 1.0f);
}

void WindowControlIcons::drawButtonFrame(
    const SDL_FRect& buttonRect,
    RC2D_Color fillColor,
    RC2D_Color borderColor) const
{
    rc2d_graphics_setColor(fillColor);
    rc2d_graphics_rectangle("fill", &buttonRect);
    rc2d_graphics_setColor(borderColor);
    rc2d_graphics_rectangle("line", &buttonRect);
}

void WindowControlIcons::drawCenteredIcon(const RC2D_Image& icon, const SDL_FRect& buttonRect, float padding) const
{
    if (icon.sdl_texture == nullptr)
    {
        return;
    }

    float iconWidth = 0.0f;
    float iconHeight = 0.0f;
    if (!SDL_GetTextureSize(icon.sdl_texture, &iconWidth, &iconHeight) ||
        iconWidth <= 0.0f ||
        iconHeight <= 0.0f)
    {
        return;
    }

    const float clampedPadding = (std::max)(0.0f, padding);
    const float maxWidth = (std::max)(1.0f, buttonRect.w - (clampedPadding * 2.0f));
    const float maxHeight = (std::max)(1.0f, buttonRect.h - (clampedPadding * 2.0f));
    const float scale = (std::min)(maxWidth / iconWidth, maxHeight / iconHeight);
    const float drawWidth = iconWidth * scale;
    const float drawHeight = iconHeight * scale;
    const float drawX = std::round(buttonRect.x + ((buttonRect.w - drawWidth) * 0.5f));
    const float drawY = std::round(buttonRect.y + ((buttonRect.h - drawHeight) * 0.5f));

    RC2D_Image imageCopy = icon;
    const RC2D_Quad quad = rc2d_graphics_newQuad(&imageCopy, 0.0f, 0.0f, iconWidth, iconHeight);
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
