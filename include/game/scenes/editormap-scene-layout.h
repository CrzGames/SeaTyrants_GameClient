#pragma once

#if GAME_ENV_DEV

namespace EditorMapSceneLayout
{
/**
 * Reserve une bande basse pour les toolbars des scenes editormap tout en
 * preservant les marges gameplay precedentes.
 */
void pushBottomToolbarPlayfieldMargins(void);

/**
 * Restaure les marges de playfield precedentes a la sortie des scenes
 * editormap.
 */
void popBottomToolbarPlayfieldMargins(void);
}

#endif
