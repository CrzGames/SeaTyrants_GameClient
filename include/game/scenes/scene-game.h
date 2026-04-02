#pragma once

#include <RC2D/RC2D.h>

#include "game/scenes/scene.h"

class GameScene : public Scene {
private:
    typedef struct OceanUniforms {
        float params0[4];
        float params1[4];
    } OceanUniforms;

    RC2D_Image waterTileImage;
    RC2D_Image causticTileImage;
    RC2D_TP_Atlas elite27Atlas;

    RC2D_GPUShader* oceanFragmentShader;
    SDL_GPURenderState* oceanRenderState;
    SDL_GPUSampler* oceanRepeatSampler;
    OceanUniforms oceanUniforms;
    double oceanTimeAccum;

    int eliteFrameIndex;
    double eliteFrameAccumulator;

    bool initializeOceanRenderState(void);
    void destroyOceanRenderState(void);
    void updateOceanUniforms(int outputWidth, int outputHeight, double dt);

public:
    GameScene(void);

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(const char *key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
};
