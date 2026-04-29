#include "game/shaders/gameplay-shader-controller.h"

#include "core/context.h"
#include "game/entities/player.h"

void GameplayShaderController::loadAll(void)
{
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();

    if (!oceanShader.load(OceanShader::WaterColor::TURQUOISE))
    {
        RC2D_log(RC2D_LOG_ERROR, "GameplayShaderController: echec chargement ocean shader");
    }
    if (!fogOfWarShader.load())
    {
        RC2D_log(RC2D_LOG_WARN, "GameplayShaderController: echec chargement fog-of-war shader");
    }
}

void GameplayShaderController::unloadAll(void)
{
    GetOceanShader().unload();
    GetFogOfWarShader().unload();
}

void GameplayShaderController::updateVisibility(double dt, const Player& player, bool fogOfWarEnabled)
{
    OceanShader& oceanShader = GetOceanShader();
    FogOfWarShader& fogOfWarShader = GetFogOfWarShader();

    // Parametres visuels gardes inline pour limiter le bruit dans la scene.
    if (fogOfWarEnabled)
    {
        fogOfWarShader.update(dt, oceanShader.getColorMode(), player.getTilePosition(), player.getViewRangeTiles(), 4.5f);
    }
}
