#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <array>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/ships/ship.h"
#include "game/ui/hud/background-widget.h"
#include "game/ui/overlay/tile-click-marker-overlay.h"

/**
 * @class EditorMapAnchorShipScene
 * @brief Scene outil dediee au reglage des anchors de sprites navire.
 *
 * Fonctionnalites principales:
 * - import d'un dossier navire (1.png ... 8.png requis);
 * - edition visuelle d'ancre par sprite;
 * - validation sprite par sprite;
 * - check automatique par preview de deplacement sur 8 directions;
 * - export d'un ship_anchor.json compatible runtime.
 */
class EditorMapAnchorShipScene : public Scene {
private:
    /**
     * @struct ShipFrame
     * @brief Donnees d'un sprite navire charge.
     */
    struct ShipFrame {
        RC2D_Image image; /**< Texture sprite. */
        float widthPx; /**< Largeur native texture. */
        float heightPx; /**< Hauteur native texture. */
        bool loaded; /**< True si la texture est valide. */
    };

    struct ImportedShip {
        std::string displayName; /**< Libelle UI du dossier navire. */
        std::string folderAbsolutePath; /**< Chemin absolu du dossier source. */
    };

    BackgroundWidget backgroundWidget; /**< Fond UI ingame (haut/bas). */
    RC2D_Font overlayFont; /**< Police overlay/outils. */
    std::array<ShipFrame, 8> shipFrames; /**< Sprites 1..8 du navire. */
    std::array<SDL_FPoint, 8> spriteAnchors; /**< Anchors normalises [0..1] par sprite. */

    int selectedSpriteIndex; /**< Index sprite courant [0..7]. */
    int selectedOceanColorIndex; /**< Index couleur ocean active. */
    int pendingOceanColorDelta; /**< Delta ocean en attente d'application. */
    float cameraZoomBeforeCheck; /**< Zoom camera memorise avant le check deplacement. */
    bool showAnchorGuides; /**< Affiche/masque les guides (carre + ancre + croix cyan). */
    bool previewIsoGridVisible; /**< Affiche/masque une grille isometrique debug 50x50. */
    float spritePreviewZoom; /**< Zoom du sprite en mode edition anchor. */
    std::string loadedShipFolderAbsolute; /**< Dossier source navire selectionne. */
    std::string statusMessage; /**< Message d'etat affiche a l'ecran. */
    std::vector<ImportedShip> importedShips; /**< Dossiers navire valides trouves dans assets/images/ships. */
    std::vector<std::string> pendingShipFolderScanPaths; /**< File de scan progressif des dossiers navire. */
    int pendingShipFolderScanIndex; /**< Index courant dans la file de scan. */
    bool shipFolderScanCompleted; /**< True quand le scan progressif est termine. */
    int selectedImportedShipIndex; /**< Index navire selectionne dans la liste. */
    int shipListScrollOffset; /**< Scroll vertical de la liste navires. */
    bool shipListScrollDragActive; /**< True pendant le drag du thumb de scroll. */
    float shipListScrollDragGrabOffsetY; /**< Offset souris/thumb pendant le drag. */

    bool pendingFolderDialogCompleted; /**< True si callback dossier recu. */
    bool pendingFolderDialogCanceled; /**< True si selection dossier annulee. */
    std::string pendingFolderAbsolute; /**< Dossier absolu recu du callback. */
    mutable std::mutex pendingFolderMutex; /**< Mutex callback -> update scene. */

    bool movementPreviewActive; /**< True si preview "check" en cours. */
    SDL_FPoint movementPreviewBaseTile; /**< Tuile centrale de preview. */
    SDL_FPoint movementPreviewCurrentTile; /**< Tuile courante issue de la simulation ship. */
    std::array<SDL_FPoint, 8> movementPreviewTargets; /**< Directions de clic simulees (8 directions ecran). */
    int movementPreviewTargetIndex; /**< Index courant dans la sequence de clics simules. */
    int movementPreviewVisualPass; /**< Pass visuelle check: 0=sprites 1..4, 1=sprites 5..8. */
    double movementPreviewPauseRemainingSec; /**< Pause restante apres chaque arrivee de clic simule. */
    Ship movementPreviewShip; /**< Ship de simulation pour reutiliser le vrai pathfinding A*. */
    TileClickMarkerOverlay clickMarker; /**< Marqueur visuel des clics simules pendant le check. */

    SDL_FRect buttonImportFolderRect; /**< Bouton import dossier navire. */
    SDL_FRect buttonPrevSpriteRect; /**< Bouton sprite precedent. */
    SDL_FRect buttonNextSpriteRect; /**< Bouton sprite suivant. */
    SDL_FRect buttonCheckMovementRect; /**< Bouton check deplacement auto. */
    SDL_FRect buttonStopCheckRect; /**< Bouton stop check deplacement (haut-droite map). */
    SDL_FRect buttonSaveAnchorRect; /**< Bouton sauvegarde ship_anchor.json. */
    SDL_FRect buttonResetAnchorRect; /**< Bouton reset ancre sprite courant. */
    SDL_FRect buttonToggleGuidesRect; /**< Bouton toggle affichage guides anchor. */
    SDL_FRect buttonToggleIsoGridRect; /**< Bouton toggle grille isometrique 50x50. */
    SDL_FRect buttonOceanPrevRect; /**< Bouton ocean precedent. */
    SDL_FRect buttonOceanNextRect; /**< Bouton ocean suivant. */
    SDL_FRect shipListRect; /**< Panel liste navires bas-droite. */

    static EditorMapAnchorShipScene* activeInstance; /**< Instance active pour callbacks async. */

    /** @brief Reinitialise l'etat runtime de l'editeur anchor. */
    void resetEditorState(void);
    /** @brief Libere toutes les textures navire chargees. */
    void unloadShipFrames(void);
    /** @brief Cree les dossiers user utilises par la scene. */
    void ensureUserStorageFolders(void);
    /** @brief Applique la couleur ocean selectionnee sur le shader ocean. */
    void applySelectedOceanColor(void);
    /** @brief Empile une demande de decalage de couleur ocean.
     *  @param delta Pas de rotation couleur (+1/-1).
     */
    void requestOceanColorStep(int delta);
    /** @brief Applique le delta ocean en attente (si present). */
    void applyPendingOceanColorStep(void);
    /** @brief Fait tourner directement la couleur ocean.
     *  @param delta Pas de rotation couleur (+1/-1).
     */
    void cycleOceanColor(int delta);
    /** @brief Lance un scan progressif des dossiers navire de assets/images/ships. */
    void beginShipFolderBatchScan(void);
    /** @brief Scanne quelques dossiers navire par frame pour eviter un pic CPU. */
    void processShipFolderBatchScan(void);
    /** @brief Tente d'ajouter un dossier navire a la liste importee. */
    bool tryAppendImportedShipFolder(const std::string& folderAbsolutePath);
    /** @brief Selectionne un navire dans la liste et le charge. */
    bool selectImportedShipAtIndex(int shipIndex);
    /** @brief Restaure le zoom camera memorise avant le check deplacement. */
    void restoreCameraZoomAfterCheck(void);
    /** @brief Ouvre le dialogue de selection dossier navire. */
    void openShipFolderDialog(void);
    /** @brief Traite le resultat differe du dialogue dossier. */
    void processPendingFolderRequest(void);
    /** @brief Charge un navire depuis un dossier absolu.
     *  @param folderAbsolutePath Dossier source.
     *  @return true si 1..8 sont valides et charges.
     */
    bool loadShipFolderFromAbsolutePath(const char* folderAbsolutePath);
    /** @brief Charge les anchors depuis ship_anchor.json (si present).
     *  @param folderPath Dossier du navire.
     *  @return true si au moins une ancre a ete lue.
     */
    bool loadAnchorsFromJsonIfPresent(const std::string& folderPath);
    /** @brief Sauvegarde les anchors en ship_anchor.json.
     *  @return true si ecriture reussie.
     */
    bool saveAnchorsToJson(void);
    /** @brief Lance le check auto deplacement 8 directions. */
    void startMovementPreviewCheck(void);
    /** @brief Repositionne le marqueur persistant sur la tuile de reference. */
    void showPersistentAnchorTileMarker(void);
    /** @brief Synchronise le Ship de preview avec le sprite actuellement edite. */
    void syncStaticPreviewShipToSelectedSprite(void);
    /** @brief Met a jour l'animation de check deplacement.
     *  @param dt Delta time secondes.
     */
    void updateMovementPreview(double dt);
    /** @brief Recalcule les rectangles des boutons UI. */
    void updateToolbarLayout(void);
    /** @brief Retourne le scroll max d'une liste simple. */
    int getShipListMaxScrollOffset(int itemCount) const;
    /** @brief Clamp le scroll de liste. */
    void clampShipListScrollOffset(int* scrollOffset, int itemCount) const;
    /** @brief Assure qu'une selection reste visible dans la liste. */
    void ensureShipSelectionVisible(int selectedIndex, int* scrollOffset, int itemCount) const;
    /** @brief Retourne l'offset de depart reel de la liste. */
    int computeShipListStartIndex(int scrollOffset, int itemCount) const;
    /** @brief Gere un clic dans le panel liste navires. */
    bool handleShipListPanelClick(float x, float y, int* outClickedIndex);
    /** @brief Met a jour le drag du scroll de liste. */
    void handleShipListPanelScrollDrag(void);
    /** @brief Dessine un bouton toolbar standard. */
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    /** @brief Dessine le panel scrollable des navires. */
    void drawShipListPanel(void) const;
    /** @brief Dessine une grille isometrique debug 50x50 sous la tuile de preview. */
    void drawPreviewIsoGrid(void) const;
    /** @brief Dessine le sprite courant + guides anchor. */
    void drawShipAnchorPreview(void) const;
    /** @brief Dessine le HUD texte + boutons. */
    void drawHud(void) const;
    /** @brief Gere un clic bouton/panneau.
     *  @return true si clic consomme.
     */
    bool handleToolbarClick(float x, float y);
    /** @brief Defini l'ancre du sprite courant depuis un clic visuel.
     *  @return true si une ancre a ete appliquee.
     */
    bool setCurrentAnchorFromClick(float x, float y);
    /** @brief Test point dans rectangle. */
    bool pointInRect(float x, float y, const SDL_FRect& rect) const;
    /** @brief Construit le rectangle de rendu du sprite courant.
     *  @param outRect Rect resultat.
     *  @param outAnchorScreenX [out] X ecran de l'ancre, si non nul.
     *  @param outAnchorScreenY [out] Y ecran de l'ancre, si non nul.
     *  @param outScale [out] Echelle appliquee au sprite, si non nul.
     *  @return true si un sprite courant est charge.
     */
    bool tryBuildCurrentSpriteDrawRect(
        SDL_FRect* outRect,
        float* outAnchorScreenX,
        float* outAnchorScreenY,
        float* outScale) const;

    /** @brief Callback async de selection dossier navire. */
    static void onOpenShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);

public:
    /** @brief Constructeur scene editor anchor navire. */
    EditorMapAnchorShipScene(void);
    /** @brief Destructeur scene editor anchor navire. */
    ~EditorMapAnchorShipScene(void) override;

    /** @brief Decharge les ressources scene. */
    void unload(void) override;
    /** @brief Charge les ressources scene. */
    void load(void) override;
    /** @brief Update frame scene.
     *  @param dt Delta time en secondes.
     */
    void update(double dt) override;
    /** @brief Draw frame scene. */
    void draw(void) override;
    /** @brief Callback clavier scene. */
    void keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    /** @brief Callback souris scene. */
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
    /** @brief Callback molette scene. */
    void mousewheelmoved(
        RC2D_MouseWheelDirection direction,
        float x,
        float y,
        Sint32 integer_x,
        Sint32 integer_y,
        float mouse_x,
        float mouse_y,
        SDL_MouseID mouseID) override;
};

#endif // GAME_ENV_DEV
