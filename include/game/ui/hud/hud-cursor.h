#pragma once

/**
 * @brief Curseurs logiques utilises par le HUD gameplay.
 *
 * Les widgets ne pilotent plus directement SDL: ils exposent seulement
 * l'intention de curseur voulue, puis l'overlay applique la decision finale.
 */
enum class HudCursorType : int {
    NONE = 0,          /**< Le widget ne demande aucun curseur particulier. */
    DEFAULT = 1,       /**< Curseur standard. */
    POINTER = 2,       /**< Main / element cliquable. */
    TEXT = 3,          /**< Curseur de saisie texte. */
    MOVE = 4,          /**< Deplacement de fenetre. */
    RESIZE_HORIZONTAL, /**< Redimensionnement / glissement horizontal. */
    RESIZE_VERTICAL,   /**< Redimensionnement / glissement vertical. */
    RESIZE_DIAGONAL    /**< Redimensionnement diagonal. */
};
