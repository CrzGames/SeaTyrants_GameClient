#include "game/shaders/gameplay-shader-controller.h"

#include "core/context.h"
#include "game/entities/player.h"

void GameplayShaderController::loadAll(void)
{
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();

    if (!oceanShader.load(OceanShader::WaterColor::TURQUOISE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameplayShaderController: echec chargement ocean shader");
    }
    if (!fogOfWarShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameplayShaderController: echec chargement fog-of-war shader");
    }
    if (!visionCloudShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameplayShaderController: echec chargement vision-cloud shader");
    }
}

void GameplayShaderController::unloadAll(void)
{
    GetOceanShader().unload();
    GetVisionCloudShader().unload();
    GetFogOfWarShader().unload();
}

void GameplayShaderController::updateVisibility(double dt, const Player& player, bool fogOfWarEnabled)
{
    OceanShader& oceanShader = GetOceanShader();
    VisionCloudShader& visionCloudShader = GetVisionCloudShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();

    // Parametres visuels gardes inline pour limiter le bruit dans la scene.
    visionCloudShader.update(dt, player.getTilePosition(), player.getViewRangeTiles(), 5.5f);
    if (fogOfWarEnabled)
    {
        fogOfWarShader.update(dt, oceanShader.getColorMode(), player.getTilePosition(), player.getViewRangeTiles(), 4.5f);
    }
}
