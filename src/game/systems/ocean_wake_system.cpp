#include "game/systems/ocean_wake_system.h"

#include <algorithm>
#include <cmath>

OceanWakeSystem::OceanWakeSystem(void)
    : wakeStamps{},
      trackers{},
      wakeStampSpacingPx(8.0f),
      wakeLifetimeSeconds(1.6f)
{
}

void OceanWakeSystem::reset(void)
{
    wakeStamps.clear();
    trackers.clear();
}

OceanWakeSystem::ShipTracker& OceanWakeSystem::getOrCreateTracker(uint64_t shipId)
{
    for (ShipTracker& tracker : trackers)
    {
        if (tracker.shipId == shipId)
        {
            return tracker;
        }
    }

    ShipTracker tracker = {};
    tracker.shipId = shipId;
    tracker.lastScreen = SDL_FPoint{0.0f, 0.0f};
    tracker.initialized = false;
    tracker.seenThisFrame = false;
    trackers.push_back(tracker);
    return trackers.back();
}

void OceanWakeSystem::beginFrame(double dt)
{
    const float deltaSeconds = static_cast<float>(dt);
    for (WakeStamp& stamp : wakeStamps)
    {
        stamp.ageSeconds += deltaSeconds;
    }

    wakeStamps.erase(
        std::remove_if(
            wakeStamps.begin(),
            wakeStamps.end(),
            [this](const WakeStamp& stamp) {
                return stamp.ageSeconds >= wakeLifetimeSeconds;
            }),
        wakeStamps.end());

    for (ShipTracker& tracker : trackers)
    {
        tracker.seenThisFrame = false;
    }
}

void OceanWakeSystem::submitShipSample(
    uint64_t shipId,
    const Map& map,
    const SDL_FPoint& tilePosition,
    bool moving)
{
    const SDL_FPoint currentScreen = map.tileToScreenCenterFloat(tilePosition.x, tilePosition.y);
    ShipTracker& tracker = getOrCreateTracker(shipId);
    tracker.seenThisFrame = true;

    if (!tracker.initialized)
    {
        tracker.lastScreen = currentScreen;
        tracker.initialized = true;
        return;
    }

    const float deltaX = currentScreen.x - tracker.lastScreen.x;
    const float deltaY = currentScreen.y - tracker.lastScreen.y;
    const float distSq = (deltaX * deltaX) + (deltaY * deltaY);
    const float spacingSq = wakeStampSpacingPx * wakeStampSpacingPx;

    if (moving && distSq >= spacingSq)
    {
        const float length = std::sqrt(distSq);
        WakeStamp stamp = {};
        stamp.tileX = tilePosition.x;
        stamp.tileY = tilePosition.y;
        stamp.dirX = deltaX / length;
        stamp.dirY = deltaY / length;
        stamp.ageSeconds = 0.0f;
        wakeStamps.push_back(stamp);

        tracker.lastScreen = currentScreen;
    }
    else if (!moving && distSq > 0.1f)
    {
        // Synchronise la base de comparaison a l'arret.
        tracker.lastScreen = currentScreen;
    }
}

void OceanWakeSystem::endFrame(
    const Map& map,
    const SDL_FRect& visibleRect,
    OceanShader& oceanShader)
{
    trackers.erase(
        std::remove_if(
            trackers.begin(),
            trackers.end(),
            [](const ShipTracker& tracker) {
                return !tracker.seenThisFrame;
            }),
        trackers.end());

    if (visibleRect.w <= 1.0f || visibleRect.h <= 1.0f)
    {
        oceanShader.clearWakePoints();
        return;
    }

    if (wakeStamps.empty())
    {
        oceanShader.clearWakePoints();
        return;
    }

    std::vector<OceanShader::WakePoint> wakePoints;
    wakePoints.reserve(static_cast<size_t>(OceanShader::MAX_WAKE_POINTS));

    for (auto it = wakeStamps.rbegin(); it != wakeStamps.rend(); ++it)
    {
        if (wakePoints.size() >= static_cast<size_t>(OceanShader::MAX_WAKE_POINTS))
        {
            break;
        }

        const SDL_FPoint wakeScreen = map.tileToScreenCenterFloat(it->tileX, it->tileY);
        const float uvX = (wakeScreen.x - visibleRect.x) / visibleRect.w;
        const float uvY = (wakeScreen.y - visibleRect.y) / visibleRect.h;
        const float age01 = std::clamp(it->ageSeconds / wakeLifetimeSeconds, 0.0f, 1.0f);

        OceanShader::WakePoint point = {};
        point.uvX = uvX;
        point.uvY = uvY;
        point.dirX = it->dirX;
        point.dirY = it->dirY;
        point.intensity = 1.0f - age01;
        point.age01 = age01;
        wakePoints.push_back(point);
    }

    if (wakePoints.empty())
    {
        oceanShader.clearWakePoints();
        return;
    }

    oceanShader.setWakePoints(wakePoints);
}

void OceanWakeSystem::setWakeStampSpacingPx(float spacingPx)
{
    if (spacingPx > 0.0f)
    {
        wakeStampSpacingPx = spacingPx;
    }
}

void OceanWakeSystem::setWakeLifetimeSeconds(float lifetimeSeconds)
{
    if (lifetimeSeconds > 0.0f)
    {
        wakeLifetimeSeconds = lifetimeSeconds;
    }
}
