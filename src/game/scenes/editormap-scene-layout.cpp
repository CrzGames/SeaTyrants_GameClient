#if GAME_ENV_DEV

#include "game/scenes/editormap-scene-layout.h"

#include <algorithm>
#include <cmath>

#include "core/context.h"
#include "game/map/map.h"

namespace
{
constexpr float kEditorMapBottomToolbarMinHeightPx = 70.0f;

int g_editorMapBottomToolbarMarginsDepth = 0;
MapPlayfieldFrameMarginsPercent g_savedEditorMapPlayfieldMargins{};

int computeEditorBottomMarginPercent(void)
{
    const float screenHeight = (std::max)(GetGameScreen().rect.h, 1.0f);
    const float percent =
        std::ceil((kEditorMapBottomToolbarMinHeightPx * 100.0f) / screenHeight);
    return static_cast<int>(std::clamp(percent, 0.0f, 20.0f));
}
}

namespace EditorMapSceneLayout
{
void pushBottomToolbarPlayfieldMargins(void)
{
    if (g_editorMapBottomToolbarMarginsDepth <= 0)
    {
        g_savedEditorMapPlayfieldMargins = MapGetPlayfieldFrameMarginsPercent();
    }

    ++g_editorMapBottomToolbarMarginsDepth;

    MapPlayfieldFrameMarginsPercent appliedMargins = g_savedEditorMapPlayfieldMargins;
    appliedMargins.bottom =
        (std::max)(appliedMargins.bottom, computeEditorBottomMarginPercent());
    MapSetPlayfieldFrameMarginsPercent(appliedMargins);
}

void popBottomToolbarPlayfieldMargins(void)
{
    if (g_editorMapBottomToolbarMarginsDepth <= 0)
    {
        return;
    }

    --g_editorMapBottomToolbarMarginsDepth;
    if (g_editorMapBottomToolbarMarginsDepth == 0)
    {
        MapSetPlayfieldFrameMarginsPercent(g_savedEditorMapPlayfieldMargins);
    }
}
}

#endif
