#pragma once

#if GAME_ENV_DEV

#include <RC2D/RC2D.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "game/scenes/scene.h"
#include "game/ships/ship.h"
#include "game/ui/overlay/scroll-bar-overlay.h"

/**
 * @class EditorMapVfxScene
 * @brief Editeur dedie a l'assemblage navire + VFX et au resize de sprites VFX.
 *
 * Deux modes principaux:
 * - SHIP_VFX: navire centre, deux listes (navires/VFX atlas), placement et edition
 *   de plusieurs instances d'animations avec export JSON.
 * - LOOSE_SPRITES: import de dossiers de sprites PNG sans spritesheet, preview
 *   de tout le dossier au centre, resize par pas de 5% et export des fichiers.
 */
class EditorMapVfxScene : public Scene {
private:
    enum class EditorMode {
        SHIP_VFX = 0,
        LOOSE_SPRITES = 1
    };

    enum class LoosePreviewMode {
        CENTER_SPRITESHEET = 0,
        PLACEMENT_PREVIEW = 1
    };

    struct ImportedShip {
        std::string displayName;
        std::string folderAbsolutePath;
        std::string configJsonPath;
    };

    struct ImportedSfxFrame {
        int index;
        std::string frameName;
        float x;
        float y;
        float w;
        float h;
    };

    struct ImportedSfx {
        std::string id;
        std::string displayName;
        std::string sourceJsonPath;
        std::string sourceImagePath;
        std::string sourceFolderAbsolutePath;
        std::string storageImagePath;
        RC2D_Image image;
        std::vector<ImportedSfxFrame> frames;
        float defaultFps;
        /**
         * Duree max d'affichage en PILOTER (ms) depuis activation du bouton ; l'anim tourne toujours au defaultFps.
         * true ou ms 0 : pas de masquage (preview identique hors pilot).
         */
        bool animationTotalDurationInfinite = true;
        int animationTotalDurationMs = 0;
    };

    struct DirectionOverride {
        bool enabled;
        float offsetX;
        float offsetY;
        float rotationDeg;
        bool flipHorizontal;
        bool flipVertical;
        int drawOrder;
        bool visible;
    };

    struct ShipVfxInstance {
        uint32_t instanceId;
        std::string label;
        std::string sourceJsonPath;
        std::string sourceDisplayName;
        int importedSfxIndex;
        float offsetX;
        float offsetY;
        float rotationDeg;
        bool flipHorizontal;
        bool flipVertical;
        int drawOrder;
        bool visible;
        bool debugBoundsVisible;
        bool locked;
        bool behindShip;
        bool followShip;
        /** Editor: mode TILE + clic carte ; fantome sur la tuile sous le curseur (meme si d'autres VFX a l'ecran). */
        bool placementSnapClickToTile;
        bool sharedForAllDirections;
        bool sharedForAllStates;
        /** 0 = pas de decalage ; sinon id d'une autre instance (meme animation) sur la page. */
        uint32_t spawnAfterInstanceId = 0;
        /** Delai apres la phase de l'instance reference (ms), pour la lecture preview / jeu. */
        int spawnAfterDelayMs = 0;
        /**
         * Preview editor: decal SPAWN (meme unite que offsetX/Y) pour placer les rejets de trainee au sol.
         */
        bool motionSpawnCaptured = false;
        float motionSpawnOffsetX = 0.0f;
        float motionSpawnOffsetY = 0.0f;
        /** 0 = pas de trainee ; sinon distance minimale (tuiles) entre deux rejets en marche. */
        int motionTrailEveryNTiles = 0;
        /** Longueur de trainee conservee par rejet (en tuiles parcourues). */
        int motionTrailLifetimeTiles = 12;
        /**
         * Si true : en marche, ancrage sur les tuiles du parcours + meme decal X/Y que le layer (comme la preview sur le navire).
         * Si false : rejets echantillonnes dans un cone arriere base sur l'offset courant du layer
         *   (voir motionTrailLateralJitterRadius), puis dispersion dans le cone.
         */
        bool motionTrailStrictTilePlacement = true;
        /**
         * Rayon du cone arriere en mode non strict (meme unite que offsetX/Y) :
         * controle la largeur et la profondeur max du nuage de spawn ; ignore si strict.
         */
        float motionTrailLateralJitterRadius = 0.0f;
        /** Decal X local du point d'origine du cone par rapport au layer (meme unite que offsetX/Y). */
        float motionTrailConeOffsetX = 0.0f;
        /** Decal Y local du point d'origine du cone par rapport au layer (meme unite que offsetX/Y). */
        float motionTrailConeOffsetY = 0.0f;
        /** Orientation du cone (deg) relative a l'arriere de la marche : 0 = plein arriere. */
        float motionTrailConeDirectionOffsetDeg = 0.0f;
        /** Demi-angle d'ouverture du cone (deg), controle la largeur (serre/large). */
        float motionTrailConeHalfAngleDeg = 28.0f;
        /** Nombre de rejets aleatoires par declenchement (mode cone). */
        int motionTrailConeSpawnCount = 1;
        /**
         * 0-100 : intensite d'une rotation aleatoire par rejet au spawn (100 % = jusqu'a Ã‚Â±45 deg par rapport au layer).
         */
        int motionTrailRotationRandomPercent = 0;
        /**
         * Pilotage : si true et navire immobile, salves de rejets en couronne autour du layer (offset courant).
         */
        bool motionTrailIdleRingWhenStationary = false;
        /** Distance du centre du layer au cercle de base des pieces (memes unites qu'offsetX/Y sur la carte). */
        float motionTrailIdleRingRadius = 48.0f;
        /** Delai entre deux salves couronne a l'arret, pilotage actif (ms). */
        int motionTrailIdleSpawnPeriodMs = 600;
        /** Nombre de pieces par salve couronne (pilotage a l'arret), typ. 4-16. */
        int motionTrailIdleRingPieceCount = 8;
        /**
         * 0-100 : rotation aleatoire par piece couronne (independant de la trainee en marche ; 100 % = jusqu'a +-45 deg).
         */
        int motionTrailIdleRingRotationRandomPercent = 0;
        /**
         * Rayon max d'un decal 2D aleatoire ajoute a chaque piece sur la couronne (memes unites qu'offsetX/Y ; 0 = cercle regulier).
         */
        float motionTrailIdleRingPositionJitterRadius = 0.0f;
        /** Accumulateur de distance (tuiles) depuis le dernier rejet en marche ; non serialise. */
        float motionTrailDistanceAcc = 0.0f;
        /** Accumulateur temps pour couronne a l'arret ; non serialise. */
        float motionTrailIdleSpawnAccSec = 0.0f;
        /**
         * Salve couronne : pieces encore a apparaitre (apres la premiere) ; non serialise.
         */
        int motionTrailIdleRingSalvoPiecesRemaining = 0;
        /** Delai interne entre deux pieces d'une meme salve ; non serialise. */
        float motionTrailIdleRingSalvoStaggerAccSec = 0.0f;
        std::array<DirectionOverride, 4> directionOverrides;
    };

    struct InvalidAssetEntry {
        std::string folderName;
        std::string reason;
    };

    struct ImportedLooseSprite {
        std::string fileName;
        std::string storagePath;
        RC2D_Image image;
        float widthPx;
        float heightPx;
    };

    struct ImportedLooseFolder {
        std::string displayName;
        std::string folderAbsolutePath;
        std::string storageFolderPath;
        std::vector<ImportedLooseSprite> sprites;
        /** Union des pixels opaques (meme logique que l'export), pour preview mode spritesheet. */
        int looseUnionCropX = 0;
        int looseUnionCropY = 0;
        int looseUnionCropW = 0;
        int looseUnionCropH = 0;
        bool looseUnionCropReady = false;
    };

    struct LoosePreviewPlacement {
        uint32_t instanceId;
        float tileX;
        float tileY;
        float fps;
        /** Horloge SDL_GetTicks (s) au spawn ; utilise avec duree totale ms > 0 pour retirer l'instance. */
        float spawnTimeSeconds = 0.0f;
    };

    RC2D_Image backgroundUiImage;
    RC2D_Font overlayFont;
    ScrollBarOverlay scrollBarOverlay;

    EditorMode editorMode;
    int selectedOceanColorIndex;
    int pendingOceanColorDelta;

    std::vector<ImportedShip> importedShips;
    std::vector<ImportedSfx> importedSfx;
    std::vector<ImportedLooseFolder> importedLooseFolders;

    int selectedShipIndex;
    int selectedSfxIndex;
    int selectedLooseFolderIndex;
    int shipListScrollOffset;
    int sfxListScrollOffset;
    int looseListScrollOffset;
    bool shipListScrollDragActive;
    bool sfxListScrollDragActive;
    bool looseListScrollDragActive;
    float shipListScrollDragGrabOffsetY;
    float sfxListScrollDragGrabOffsetY;
    float looseListScrollDragGrabOffsetY;

    Ship previewShip;
    bool previewShipLoaded;
    std::string loadedShipFolderAbsolute;
    SDL_FPoint previewShipTile;
    int previewDirectionIndex;
    int previewShipStateIndex;
    /** Opacite preview navire 0..100 (pas de 10), appliquee via setDrawAlpha. */
    int previewShipOpacityPercent;
    /** Une entree par page calques (direction x etat HP), cle = previewDirectionIndex + previewShipStateIndex * 4. */
    std::array<int, 8> shipDrawOrderByPage{};
    std::array<bool, 8> shipLayerVisibleByPage{{true, true, true, true, true, true, true, true}};
    std::array<bool, 8> shipLayerLockedByPage{};
    std::array<bool, 8> shipDebugBoundsVisibleByPage{{true, true, true, true, true, true, true, true}};
    /** Par page : navire visible dans la preview seulement apres ce delai (ms) dans le cycle du VFX reference. */
    std::array<uint32_t, 8> shipSpawnAfterVfxInstanceId{};
    std::array<int, 8> shipSpawnAfterDelayMs{};
    bool shipLayerSelected;
    /** Debug : grille iso 10x10 au sol sous le navire preview. */
    bool previewIsoGridVisible;
    RC2D_Image looseReferenceGuildIslandImage;
    RC2D_Image looseReferenceTowerLevel1Image;
    RC2D_Image looseReferenceTowerLevel2Image;
    RC2D_Image looseReferenceTowerLevel3Image;
    RC2D_Image looseReferenceTowerLevel4Image;
    RC2D_Image looseReferenceShipLeftImage;
    RC2D_Image looseReferenceShipRightImage;
    bool looseReferencePreviewVisible;
    bool looseReferencePreviewLoaded;

    std::array<std::vector<ShipVfxInstance>, 8> shipVfxLayerPages{};
    bool shipVfxLayerPagePickerOpen = false;

    int getShipVfxLayerPageKey(void) const
    {
        const int dir = std::clamp(this->previewDirectionIndex, 0, 3);
        const int st = std::clamp(this->previewShipStateIndex, 0, 1);
        return dir + st * 4;
    }

    std::vector<ShipVfxInstance>& currentShipVfxLayers(void)
    {
        return this->shipVfxLayerPages[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    const std::vector<ShipVfxInstance>& currentShipVfxLayers(void) const
    {
        return this->shipVfxLayerPages[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    int& activeShipDrawOrder(void)
    {
        return this->shipDrawOrderByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    const int& activeShipDrawOrder(void) const
    {
        return this->shipDrawOrderByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    bool& activeShipLayerVisible(void)
    {
        return this->shipLayerVisibleByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    const bool& activeShipLayerVisible(void) const
    {
        return this->shipLayerVisibleByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    bool& activeShipLayerLocked(void)
    {
        return this->shipLayerLockedByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    const bool& activeShipLayerLocked(void) const
    {
        return this->shipLayerLockedByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    bool& activeShipDebugBoundsVisible(void)
    {
        return this->shipDebugBoundsVisibleByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    const bool& activeShipDebugBoundsVisible(void) const
    {
        return this->shipDebugBoundsVisibleByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    int selectedVfxInstanceIndex;
    uint32_t nextVfxInstanceId;
    std::string layerNameInput;
    bool layerNameInputFocused;
    struct VfxRelativePopupLayout
    {
        SDL_FRect dimFullMap{};
        SDL_FRect popup{};
        SDL_FRect delayInputRect{};
        SDL_FRect validateBtn{};
        SDL_FRect clearBtn{};
        SDL_FRect cancelBtn{};
        int candidateCount = 0;
        SDL_FRect candidateRows[20]{};
    };
    struct VfxTrailPopupLayout
    {
        SDL_FRect dimFullMap{};
        SDL_FRect popup{};
        /** Ligne horizontale sous la ligne instance (separe en-tete / bloc marche). */
        float trailRuleAfterInstanceY = 0.0f;
        /** Ligne horizontale avant le bloc couronne (separe marche / arret). */
        float trailRuleBeforeArretY = 0.0f;
        SDL_FRect everyNTilesInputRect{};
        SDL_FRect lifetimeTilesInputRect{};
        SDL_FRect trailStrictTileBtn{};
        SDL_FRect trailLateralSpreadBtn{};
        bool lateralSectionVisible = false;
        SDL_FRect lateralJitterInputRect{};
        SDL_FRect rotationPctInputRect{};
        SDL_FRect idleRingOffBtn{};
        SDL_FRect idleRingOnBtn{};
        bool idleRingInputsVisible = false;
        SDL_FRect idleRingPieceCountInputRect{};
        SDL_FRect idleRingPeriodMsInputRect{};
        SDL_FRect idleRingRadiusInputRect{};
        SDL_FRect idleRingRotationPctInputRect{};
        SDL_FRect idleRingPosJitterInputRect{};
        SDL_FRect validateBtn{};
        SDL_FRect clearBtn{};
        SDL_FRect cancelBtn{};
        /** Colonne droite : apercu des rejets en marche (live depuis les champs du popup). */
        SDL_FRect previewMarcheRect{};
        /** Colonne droite : apercu couronne a l'arret. */
        SDL_FRect previewArretRect{};
        /** Bouton afficher / masquer le navire dans l'apercu rejets en marche. */
        SDL_FRect previewMarcheShipToggleBtn{};
        /** Vitesse preview marche (MoveSpeedTilesPerSecond) : boutons - / +. */
        SDL_FRect previewSpeedMinusBtn{};
        SDL_FRect previewSpeedPlusBtn{};
        /** Couleur ocean (meme cycle que la barre d'outils) : pas / pas. */
        SDL_FRect previewOceanPrevBtn{};
        SDL_FRect previewOceanNextBtn{};
        /** Zoom commun aux deux previews (independant du zoom carte). */
        SDL_FRect previewZoomMinusBtn{};
        SDL_FRect previewZoomPlusBtn{};
        /** Bouton afficher / masquer le navire dans l'apercu couronne. */
        SDL_FRect previewCrownShipToggleBtn{};
    };
    struct VfxTrailConePopupLayout
    {
        SDL_FRect dimFullMap{};
        SDL_FRect popup{};
        SDL_FRect previewRect{};
        SDL_FRect centerHandleRect{};
        SDL_FRect tipHandleRect{};
        SDL_FRect sideHandleRect{};
        SDL_FRect spawnCountMinusBtn{};
        SDL_FRect spawnCountPlusBtn{};
        SDL_FRect validateBtn{};
        SDL_FRect resetBtn{};
        SDL_FRect cancelBtn{};
    };
    bool vfxRelativeTimingPopupVisible = false;
    bool vfxRelativeTimingPopupIsShipRow = false;
    int vfxRelativeTimingPopupTargetVfxIndex = -1;
    int vfxRelativeTimingPopupStep = 0;
    std::vector<int> vfxRelativeTimingPopupCandidateIndices;
    uint32_t vfxRelativeTimingPopupAnchorInstanceId = 0;
    std::string vfxRelativeTimingPopupDelayMsInput;
    bool vfxRelativeTimingPopupDelayMsFocused = false;
    mutable VfxRelativePopupLayout vfxRelativePopupLastLayout;

    bool vfxTrailPopupVisible = false;
    int vfxTrailPopupParentInstanceIndex = -1;
    std::string vfxTrailPopupEveryNTilesInput;
    std::string vfxTrailPopupLifetimeTilesInput;
    std::string vfxTrailPopupLateralJitterInput;
    std::string vfxTrailPopupRotationPctInput;
    std::string vfxTrailPopupIdleRingPieceCountInput;
    std::string vfxTrailPopupIdlePeriodMsInput;
    std::string vfxTrailPopupIdleRadiusInput;
    std::string vfxTrailPopupIdleRingRotationPctInput;
    std::string vfxTrailPopupIdleRingPosJitterInput;
    bool vfxTrailPopupStrictTilePlacement = true;
    bool vfxTrailPopupIdleRingWhenStationary = false;
    bool vfxTrailPopupEveryNTilesFocused = false;
    bool vfxTrailPopupLifetimeTilesFocused = false;
    bool vfxTrailPopupLateralJitterFocused = false;
    bool vfxTrailPopupRotationPctFocused = false;
    bool vfxTrailPopupIdleRingPieceCountFocused = false;
    bool vfxTrailPopupIdlePeriodMsFocused = false;
    bool vfxTrailPopupIdleRadiusFocused = false;
    bool vfxTrailPopupIdleRingRotationPctFocused = false;
    bool vfxTrailPopupIdleRingPosJitterFocused = false;
    /** Apercu rejets en marche : dessine le navire sur la trajectoire (toggle dans le popup). */
    bool vfxTrailPopupPreviewMarcheShipVisible = true;
    /** Apercu rejets en marche : vitesse de deplacement en tuiles/s (independante du gameplay). */
    float vfxTrailPopupPreviewMarcheSpeedTilesPerSec = 0.0f;
    /** Apercu couronne : dessine le navire au centre (toggle dans le popup). */
    bool vfxTrailPopupPreviewCrownShipVisible = true;
    /** Zoom Ã‚Â« monde Ã‚Â» des previews trainee (0.40 .. 1.00, comme la camera carte) pour navire + VFX. */
    float vfxTrailPopupPreviewZoom = 1.0f;
    mutable VfxTrailPopupLayout vfxTrailPopupLastLayout;
    bool vfxTrailConePopupVisible = false;
    int vfxTrailConePopupParentInstanceIndex = -1;
    float vfxTrailConePopupLength = 96.0f;
    float vfxTrailConePopupOffsetX = 0.0f;
    float vfxTrailConePopupOffsetY = 0.0f;
    float vfxTrailConePopupDirectionOffsetDeg = 0.0f;
    float vfxTrailConePopupHalfAngleDeg = 28.0f;
    int vfxTrailConePopupSpawnCount = 1;
    bool vfxTrailConePopupCenterDragActive = false;
    bool vfxTrailConePopupTipDragActive = false;
    bool vfxTrailConePopupSideDragActive = false;
    mutable VfxTrailConePopupLayout vfxTrailConePopupLastLayout{};

    struct ShipVfxTrailPiece {
        uint32_t sourceVfxInstanceId = 0;
        float anchorShipTileX = 0.0f;
        float anchorShipTileY = 0.0f;
        float bornTimeSeconds = 0.0f;
        float timeRemainingSec = 0.0f;
        /** Decal affiche (copie au spawn) : ne pas relire l'instance au dessin si le SPAWN est re-valide. */
        float trailDrawOffsetX = 0.0f;
        float trailDrawOffsetY = 0.0f;
        /**
         * Au spawn : decal ecran (px) centre tuile -> reference navire, identique a drawShipVfxPreview + getCurrentSpriteCenterOffsetPixels.
         * Evite d'appliquer le pivot du navire courant a une ancienne tuile d'ancrage.
         */
        float anchorShipSpriteCenterOffXPx = 0.0f;
        float anchorShipSpriteCenterOffYPx = 0.0f;
        bool anchorShipSpriteCenterOffValid = false;
        /** Decal perpendiculaire a la marche (meme unite que trailDraw), fige au spawn. */
        float trailPerpendicularJitterX = 0.0f;
        float trailPerpendicularJitterY = 0.0f;
        /** Ecart de rotation (deg) ajoute au layer pour ce rejet, fige au spawn. */
        float trailRotationJitterDeg = 0.0f;
        /** Phase initiale (s) de l'instance source au spawn, pour synchroniser piece <-> layer. */
        float trailInitialPhaseSec = 0.0f;
        /** Identifiant stable pour la ligne Ã‚Â« sous-instance Ã‚Â» dans le panneau Layers. */
        uint32_t layerPanelUiId = 0;
        /** True si cree par la couronne a l'arret ; supprime des que le navire reprend sa marche. */
        bool fromIdleRingCrown = false;
        /**
         * Preview popup Ã‚Â« en marche Ã‚Â» uniquement : si >= 0, position du rejet sur le segment [0..1]
         * (haut-droite -> bas-gauche pour page Bas-Gauche, etc.) au lieu des tuiles d'ancrage carte.
         */
        float marchePopupPreviewPathU = -1.0f;
    };
    /** Rejets de trainee / sous-instances : une liste par page calques (direction x HP), comme shipVfxLayerPages. */
    std::array<std::vector<ShipVfxTrailPiece>, 8> shipVfxTrailPiecesByPage{};
    uint32_t nextShipVfxTrailLayerPanelUiId = 1;
    /**
     * Simulation "en marche" pour la preview popup : accumulateur de distance (tuiles),
     * avec les valeurs des champs du popup (appendMotionTrailPieceFromStep).
     */
    mutable std::vector<ShipVfxTrailPiece> vfxTrailPopupMarcheSimPieces{};
    /** Position normalisee [0..1) le long du parcours preview pour la tete / le spawn (boucle). */
    mutable float vfxTrailPopupMarcheDistAlongPathPx = 0.0f;
    mutable float vfxTrailPopupMarcheSimDistanceAcc = 0.0f;
    mutable std::string vfxTrailPopupMarcheSimParamSignature{};
    mutable uint32_t vfxTrailPopupMarcheSimNextUiId = 1U;
    /** Dernier deplacement tuile (normalise) pendant PILOTER : oriente jitter comme sur la carte. */
    mutable float vfxTrailPopupMarcheLastPilotMoveDirX = 0.0f;
    mutable float vfxTrailPopupMarcheLastPilotMoveDirY = 1.0f;
    mutable bool vfxTrailPopupMarcheLastPilotMoveDirValid = false;
    std::array<SDL_FPoint, 8> shipVfxTrailPrevShipTileByPage{};
    std::array<bool, 8> shipVfxTrailPrevShipTileValidByPage{};

    std::vector<ShipVfxTrailPiece>& currentShipVfxTrailPieces(void)
    {
        return this->shipVfxTrailPiecesByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    const std::vector<ShipVfxTrailPiece>& currentShipVfxTrailPieces(void) const
    {
        return this->shipVfxTrailPiecesByPage[static_cast<size_t>(this->getShipVfxLayerPageKey())];
    }

    struct VfxDuplicateToPagesPopupLayout
    {
        SDL_FRect dimFullMap{};
        SDL_FRect popup{};
        SDL_FRect pageRowRects[8]{};
        SDL_FRect btnAll{};
        SDL_FRect btnNone{};
        SDL_FRect btnOtherPages{};
        SDL_FRect btnSourceOnly{};
        SDL_FRect validateBtn{};
        SDL_FRect cancelBtn{};
    };
    bool vfxDuplicateToPagesPopupVisible = false;
    ShipVfxInstance vfxDuplicateToPagesPopupSourceSnapshot{};
    int vfxDuplicateToPagesPopupSourcePageKey = -1;
    std::array<bool, 8> vfxDuplicateToPagesPageSelected{};
    mutable VfxDuplicateToPagesPopupLayout vfxDuplicateToPagesPopupLastLayout{};

    bool vfxDragActive;
    /** Mode cadran ROT (panneau layers) : cercle autour du navire + ligne vers le curseur. */
    bool vfxRotationDialActive;
    float vfxDragStartMouseX;
    float vfxDragStartMouseY;
    float vfxDragStartOffsetX;
    float vfxDragStartOffsetY;
    bool shipVfxDirty;
    std::string loadedShipVfxConfigPath;

    int looseScalePercent;
    float loosePreviewZoomFactor;
    /** Zoom camera mode Ship/VFX (meme plage que le downscale : 0.40..1.00). */
    float shipVfxPreviewZoomFactor;
    LoosePreviewMode loosePreviewMode;
    /** Si true, le clic preview ancre sur le centre de la tuile la plus proche ; sinon tuile flottante (sous-pixel). */
    bool loosePreviewPlacementSnapToTile;
    std::vector<LoosePreviewPlacement> loosePreviewPlacements;
    uint32_t nextLoosePreviewPlacementId;
    std::string loosePreviewFpsInput;
    bool loosePreviewFpsInputFocused;
    /** Vide = desactive ; sinon duree d'une boucle complete de l'anim (ms), pour preview + export fps derive. */
    std::string loosePreviewTotalDurationMsInput;
    bool loosePreviewTotalDurationMsInputFocused;
    bool looseExportNamePopupVisible;
    std::string looseExportNameInput;
    std::string pendingLooseExportAnimationName;
    unsigned int importedSfxCounter;
    unsigned int importedLooseFolderCounter;

    std::string statusMessage;
    std::vector<InvalidAssetEntry> invalidShipFolders;
    std::vector<InvalidAssetEntry> invalidVfxFolders;

    bool pendingShipFolderDialogCompleted;
    bool pendingShipFolderDialogCanceled;
    std::string pendingShipFolderAbsolute;
    mutable std::mutex pendingShipFolderMutex;

    bool pendingSfxFolderDialogCompleted;
    bool pendingSfxFolderDialogCanceled;
    std::string pendingSfxFolderAbsolute;
    mutable std::mutex pendingSfxFolderMutex;

    bool pendingLooseFolderDialogCompleted;
    bool pendingLooseFolderDialogCanceled;
    std::string pendingLooseFolderAbsolute;
    mutable std::mutex pendingLooseFolderMutex;

    bool pendingExportFolderDialogCompleted;
    bool pendingExportFolderDialogCanceled;
    std::string pendingExportFolderAbsolute;
    EditorMode pendingExportMode;
    mutable std::mutex pendingExportFolderMutex;

    bool pendingShipVfxConfigDialogCompleted;
    bool pendingShipVfxConfigDialogCanceled;
    std::string pendingShipVfxConfigAbsolutePath;
    mutable std::mutex pendingShipVfxConfigMutex;

    SDL_FRect buttonModeShipVfxRect;
    SDL_FRect buttonModeLooseSpritesRect;
    SDL_FRect buttonImportShipRect;
    SDL_FRect buttonImportSfxRect;
    SDL_FRect buttonReloadAssetsRect;
    SDL_FRect buttonImportLooseRect;
    SDL_FRect buttonExportRect;
    SDL_FRect buttonOceanPrevRect;
    SDL_FRect buttonOceanNextRect;

    SDL_FRect buttonDirectionPrevRect;
    SDL_FRect buttonDirectionNextRect;
    SDL_FRect buttonShipStateToggleRect;
    SDL_FRect buttonShipOpacityMinusRect;
    SDL_FRect buttonShipOpacityPlusRect;
    SDL_FRect buttonPreviewIsoGridRect;
    SDL_FRect buttonShipVfxZoomMinusRect;
    SDL_FRect buttonShipVfxZoomPlusRect;
    /** Pilotage RTS du navire preview (clic carte = moveToTile). */
    bool previewShipPilotActive;
    /** Horloge SDL_GetTicks (s) au passage PILOTER ON : clip duree max VFX par atlas (animationTotalDurationMs). */
    float pilotVfxMaxDurationClockAnchorSeconds;
    SDL_FRect buttonShipPilotRect;
    SDL_FRect buttonShipOrderMinusRect;
    SDL_FRect buttonShipOrderPlusRect;
    SDL_FRect buttonLayerOrderMinusRect;
    SDL_FRect buttonLayerOrderPlusRect;
    SDL_FRect buttonVfxOrderMinusRect;
    SDL_FRect buttonVfxOrderPlusRect;
    SDL_FRect buttonVisibleRect;
    SDL_FRect buttonLockedRect;
    SDL_FRect buttonBehindShipRect;
    SDL_FRect buttonFollowShipRect;
    SDL_FRect buttonRemoveVfxRect;
    SDL_FRect buttonDuplicateVfxRect;
    SDL_FRect buttonLayerNameInputRect;
    SDL_FRect buttonResetTransformRect;
    SDL_FRect buttonMoveULRect;
    SDL_FRect buttonMoveUpRect;
    SDL_FRect buttonMoveURRect;
    SDL_FRect buttonMoveLeftRect;
    SDL_FRect buttonMoveRightRect;
    SDL_FRect buttonMoveDLRect;
    SDL_FRect buttonMoveDownRect;
    SDL_FRect buttonMoveDRRect;

    SDL_FRect buttonLooseScaleMinusRect;
    SDL_FRect buttonLooseScalePlusRect;
    SDL_FRect buttonLooseReferencePreviewRect;
    SDL_FRect buttonLooseZoomMinusRect;
    SDL_FRect buttonLooseZoomPlusRect;
    SDL_FRect buttonLoosePreviewModeRect;
    SDL_FRect buttonLoosePreviewFpsInputRect;
    SDL_FRect buttonLoosePreviewTotalDurationMsInputRect;
    SDL_FRect buttonLooseClearAllVfxRect;
    SDL_FRect buttonLoosePreviewPlacementSnapRect;

    SDL_FRect shipListRect;
    SDL_FRect sfxListRect;
    SDL_FRect layerListRect;
    SDL_FRect looseListRect;
    SDL_FRect invalidVfxListRect;
    SDL_FRect invalidShipListRect;
    int layerListScrollOffset;
    bool layerListScrollDragActive;
    float layerListScrollDragGrabOffsetY;
    bool layerRowDragActive;
    bool layerRowDragMoved;
    int layerRowDragSourceDisplayIndex;
    int layerRowDragTargetInsertIndex;
    float layerRowDragStartMouseY;
    int invalidVfxListScrollOffset;
    int invalidShipListScrollOffset;
    bool invalidVfxListScrollDragActive;
    bool invalidShipListScrollDragActive;
    float invalidVfxListScrollDragGrabOffsetY;
    float invalidShipListScrollDragGrabOffsetY;

    static EditorMapVfxScene* activeInstance;

    void resetEditorState(void);
    /** Remet interactions (drag, focus) sans toucher aux imports ni au mode courant. */
    void clearEditorTransientInteractionState(void);
    /** Vue camÃƒÂ©ra + tuile navire comme au chargement (mode Ship / VFX). */
    void applyShipVfxModeViewportReset(void);
    /** Recharge le navire preview si decharge, sans vider les calques VFX. */
    void reloadPreviewShipIfUnloadedKeepVfxLayers(void);
    /** Etat loose (sous-modes, placements, zoom) remis a zero ; camera centree comme au chargement. */
    void applyLooseSpritesModeEntryReset(void);
    void ensureUserStorageFolders(void);
    void unloadImportedSfx(void);
    void unloadImportedLooseFolders(void);
    void loadLooseReferencePreviewAssets(void);
    void unloadLooseReferencePreviewAssets(void);
    void adjustLoosePreviewZoom(float delta);
    void adjustShipVfxPreviewZoom(float delta);
    void applySelectedOceanColor(void);
    void requestOceanColorStep(int delta);
    void applyPendingOceanColorStep(void);
    void cycleOceanColor(int delta);
    void autoImportAssetsFromDefaultFolders(void);
    std::string buildShipConfigJsonPath(const ImportedShip& ship) const;
    void applyPreviewDirectionToShip(void);
    void setPreviewDirectionIndex(int directionIndex);
    void cyclePreviewDirection(int delta);
    void cyclePreviewShipState(int delta);
    void applyPreviewShipOpacityPercentToShip(void);
    void adjustPreviewShipOpacityPercentStep(int deltaPercent);
    const char* getPreviewDirectionLabel(void) const;
    const char* getPreviewShipStateLabel(void) const;
    void markShipVfxDirty(void);
    void openVfxRelativeTimingPopup(bool forShipRow, int vfxInstanceIndex);
    void closeVfxRelativeTimingPopup(void);
    bool computeVfxRelativePopupLayout(VfxRelativePopupLayout* out) const;
    void drawVfxRelativeTimingPopup(void) const;
    bool handleVfxRelativeTimingPopupMouseClick(float x, float y, RC2D_MouseButton button);
    bool handleVfxRelativeTimingPopupKey(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);
    void openVfxTrailPopupForInstanceIndex(int vfxInstanceIndex);
    void closeVfxTrailPopup(void);
    bool computeVfxTrailPopupLayout(VfxTrailPopupLayout* out) const;
    void drawVfxTrailPopupPreviews(const VfxTrailPopupLayout& lay) const;
    void drawVfxTrailPopup(void) const;
    bool handleVfxTrailPopupMouseClick(float x, float y, RC2D_MouseButton button);
    bool handleVfxTrailPopupKey(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);
    void openVfxTrailConePopupForInstanceIndex(int vfxInstanceIndex);
    void closeVfxTrailConePopup(void);
    bool computeVfxTrailConePopupLayout(VfxTrailConePopupLayout* out) const;
    void drawVfxTrailConePopup(void) const;
    bool handleVfxTrailConePopupMouseClick(float x, float y, RC2D_MouseButton button);
    bool handleVfxTrailConePopupKey(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat);
    void updateVfxTrailConePopupDragFromMouse(void);
    std::vector<int> expandLayerPanelDisplayRows(const std::vector<int>& coreOrdered) const;
    int findVfxLayerIndexByInstanceId(uint32_t instanceId) const;
    int findTrailPieceIndexByLayerPanelUiId(uint32_t uiId) const;
    void removeTrailPiecesWithSourceInstanceId(uint32_t sourceInstanceId);
    int computeVfxPreviewFrameIndex(
        const ShipVfxInstance& instance,
        float timeSeconds,
        const std::vector<ShipVfxInstance>& layerVec,
        const ImportedSfx& imported) const;
    /** Index de frame pour une sous-instance trainee/couronne (age depuis spawn). */
    int computeTrailPieceFrameIndex(const ImportedSfx& imported, float elapsedSinceSpawnSec) const;
    bool shouldSkipDrawImportedSfxForPilotMaxLifetime(const ImportedSfx& imported, float timeSeconds) const;
    /** Phase [0, periode) en secondes pour une instance, en enchainant les RELATIF (A->B->C). */
    float computeVfxPreviewPhaseSecondsInCycle(
        const ShipVfxInstance& instance,
        float timeSeconds,
        const std::vector<ShipVfxInstance>& layerVec,
        const ImportedSfx& imported,
        int chainDepth) const;
    bool shouldPreviewHideShipForRelativeTiming(float timeSeconds) const;
    void initDefaultShipLayerSettingsAllPages(void);
    void clearAllShipVfxLayerPages(void);
    void clearShipVfxLayerUiTransientStateForPageChange(void);
    void applyShipVfxLayerPageIndex(int pageIndex);
    int getActiveDirectionIndexForOverrides(void) const;
    bool hasSelectedVfxInstance(void) const;
    ShipVfxInstance* getSelectedVfxInstance(void);
    const ShipVfxInstance* getSelectedVfxInstance(void) const;
    DirectionOverride* getEditableDirectionOverride(ShipVfxInstance* instance);
    const DirectionOverride* getResolvedDirectionOverride(const ShipVfxInstance* instance) const;
    /** Offsets logiques pour le VFX accroche au navire preview (offset courant / override). */
    void getVfxPreviewDrawOffsets(
        const ShipVfxInstance& instance,
        const DirectionOverride* resolvedOverride,
        float* outOffsetX,
        float* outOffsetY) const;
    void appendMotionTrailPieceFromStep(
        const ShipVfxInstance& inst,
        float anchorShipTileX,
        float anchorShipTileY,
        float moveDxTiles,
        float moveDyTiles,
        float timeSec,
        std::vector<ShipVfxTrailPiece>& outPieces,
        uint32_t& nextUiId,
        float marchePopupPreviewPathU = -1.0f,
        float anchorShipCenterOffsetEffectiveZoom = -1.0f,
        float spawnSpeedTilesPerSec = -1.0f) const;
    void updateVfxTrailPopupMarcheSimulation(double dt);
    void resetVfxTrailPopupMarcheSimulationState(const std::string& signature);
    void updatePreviewShipPilotAndVfxMotion(double dt);
    void togglePreviewShipPilotControl(void);
    void clearShipVfxTrailPieces(void);
    void closeVfxDuplicateToPagesPopup(void);
    void openVfxDuplicateToPagesPopupFromSelectedVfx(void);
    void applyVfxDuplicateToPagesPopupValidate(void);
    bool computeVfxDuplicateToPagesPopupLayout(VfxDuplicateToPagesPopupLayout* out) const;
    void drawVfxDuplicateToPagesPopup(void) const;
    bool handleVfxDuplicateToPagesPopupMouseClick(float x, float y, RC2D_MouseButton button);
    bool handleVfxDuplicateToPagesPopupKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
    ShipVfxInstance duplicateShipVfxInstanceFreshId(const ShipVfxInstance& src);
    bool duplicateVfxInstanceAtIndexInCurrentPage(int instanceIndex);
    void drawShipVfxTrailPieces(void) const;
    void captureVfxMotionSpawnAtIndex(int instanceIndex);
    void resetSelectedVfxTransform(void);
    void toggleSelectedVfxVisibility(void);
    void toggleSelectedVfxLock(void);
    void toggleSelectedVfxBehindShip(void);
    void toggleSelectedVfxSharedForAllDirections(void);
    void toggleSelectedVfxDirectionOverride(void);
    void moveSelectedLayerOrder(int delta);
    bool applyLayerNameInput(void);
    bool importShipVfxConfigFromPath(const char* absoluteFilePath);
    bool loadShipVfxConfigForSelectedShip(void);

    void updateToolbarLayout(void);
    void drawToolbarButton(const SDL_FRect& rect, const char* label, bool active) const;
    bool pointInRect(float x, float y, const SDL_FRect& rect) const;
    void convertWindowToRender(float windowX, float windowY, float* outX, float* outY) const;
    bool getMouseRenderPosition(float* outX, float* outY) const;

    int getListMaxScrollOffset(int itemCount) const;
    void clampListScrollOffset(int* scrollOffset, int itemCount) const;
    void ensureSelectionVisible(int selectedIndex, int* scrollOffset, int itemCount) const;
    int computeListStartIndex(int scrollOffset, int itemCount) const;
    bool handleListPanelClick(
        float x,
        float y,
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY,
        int* outClickedIndex);
    void handleListPanelScrollDragFromMouse(
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);
    void drawListPanel(
        const SDL_FRect& panelRect,
        const char* title,
        const std::vector<std::string>& labels,
        int selectedIndex,
        int scrollOffset) const;
    void drawLayerListPanel(const std::vector<int>& orderedLayerIndices) const;
    void updateLayerListRowDragFromMouse(void);
    void applyLayerPanelReorderFromDisplayDrag(int sourceDisplayIndex, int targetInsertIndex);
    void drawInvalidAssetPanel(
        const SDL_FRect& panelRect,
        const char* title,
        const std::vector<InvalidAssetEntry>& entries,
        int scrollOffset) const;
    bool handleInvalidAssetPanelClick(
        float x,
        float y,
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);
    void handleInvalidAssetPanelScrollDragFromMouse(
        const SDL_FRect& panelRect,
        int itemCount,
        int* scrollOffset,
        bool* dragActive,
        float* dragGrabOffsetY);

    void openImportShipFolderDialog(void);
    void openImportSfxFolderDialog(void);
    void openImportShipVfxConfigDialog(void);
    void openImportLooseFolderDialog(void);
    void openExportFolderDialog(void);
    void openLooseExportNamePopup(void);
    void processPendingShipFolderRequest(void);
    void processPendingSfxFolderRequest(void);
    void processPendingShipVfxConfigRequest(void);
    void processPendingLooseFolderRequest(void);
    void processPendingExportFolderRequest(void);

    bool importShipsFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);
    bool loadShipFolderFromAbsolutePath(const char* folderAbsolutePath);
    bool selectImportedShipAtIndex(int shipIndex);

    bool importSfxFromAbsolutePath(const char* absolutePath);
    bool importSfxFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);

    bool importLooseFolderFromAbsolutePath(const char* absolutePath);
    bool importLooseFoldersFromRootFolderAbsolutePath(const char* rootFolderAbsolutePath);
    bool reloadImportedLooseFolderFromAbsolutePath(const char* absolutePath);
    void refreshLooseFolderUnionCrop(ImportedLooseFolder& folder);
    void updateLoosePreviewPlacementExpirations(void);

    void spawnSelectedSfxAtShipCenter(void);
    void setSelectedVfxInstanceIndex(int index);
    void rebuildVfxLayerLabelsFromCurrentInstances(void);
    void removeSelectedVfxInstance(void);
    void centerSelectedVfxInstance(void);
    void snapSelectedVfxCenterToNearestTileAtScreen(float screenX, float screenY);
    void moveSelectedVfxInstance(float deltaX, float deltaY);
    void adjustSelectedVfxRotation(float deltaDegrees);
    void applySelectedVfxRotationFromScreenPointer(float pointerX, float pointerY);
    void toggleSelectedVfxFlipHorizontal(void);
    void toggleSelectedVfxFlipVertical(void);
    void toggleVfxInstanceFlipHorizontalAtIndex(int instanceIndex);
    void toggleVfxInstanceFlipVerticalAtIndex(int instanceIndex);
    void toggleSelectedVfxFollowShip(void);
    void adjustSelectedVfxDrawOrder(int delta);
    void adjustShipDrawOrder(int delta);
    void normalizeShipVfxDrawOrders(void);
    int findTopmostVfxInstanceIndexAtPoint(float x, float y) const;
    /** Comme findTopmost, mais ignore une instance (index a exclure, ou -1). */
    int findTopmostVfxInstanceIndexAtPointExcluding(float x, float y, int excludeInstanceIndex) const;
    /** Tuile logique sous le centre ecran du VFX selectionne (pour comparer au curseur). */
    bool tryGetScreenTileNearestForSelectedVfxCenter(SDL_Point* outTile) const;
    void drawShipVfxTilePlacementGhost(void) const;
    bool applyLoosePreviewFpsInput(void);
    float getLoosePreviewFpsOrDefault(void) const;
    int getLoosePreviewTotalDurationMsActive(void) const;
    bool applyLoosePreviewTotalDurationMsInput(void);
    int computeLooseAnimatedFrameIndex(float nowSeconds, int frameCount, float fpsFallback) const;
    float computeLooseExportSpritesheetFps(int frameCount) const;
    bool handleLayerNameInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
    bool handleLoosePreviewFpsInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
    bool handleLoosePreviewTotalDurationMsInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);
    bool handleLooseExportNameInputKey(
        const char* key,
        SDL_Scancode scancode,
        SDL_Keycode keycode,
        SDL_Keymod mod,
        bool isrepeat);

    bool exportShipVfxJsonToFolder(const char* absoluteFolderPath);
    bool exportLooseFolderScaledToFolder(const char* absoluteFolderPath, const std::string& animationName);

    void drawShipVfxDebugIsoGrid(void) const;
    void drawShipVfxPreview(void) const;
    void drawShipVfxRotationDialOverlay(void) const;
    void drawLooseReferencePreview(void) const;
    void drawLooseSpritesPreview(void) const;
    void drawLoosePlacementPreview(void) const;
    void drawLooseExportNamePopup(void) const;
    void drawHud(void) const;
    int getSelectedLayerRowIndexForDisplay(const std::vector<int>& orderedInstanceIndices) const;
    std::vector<int> getOrderedVfxInstanceIndicesForLayerPanel(void) const;

    bool handleShipListClick(float x, float y);
    bool handleSfxListClick(float x, float y);
    bool handleLayerListClick(float x, float y, RC2D_MouseButton button);
    bool handleLooseListClick(float x, float y);
    bool handleToolbarClick(float x, float y);
    bool handlePreviewClick(float x, float y, RC2D_MouseButton button);
    bool handleLoosePlacementPreviewClick(float x, float y, RC2D_MouseButton button);

    static void onImportShipFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onImportSfxFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onImportShipVfxConfigDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onImportLooseFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);
    static void onExportFolderDialogResult(void* userdata, const char* const* filelist, int filter_index);

public:
    EditorMapVfxScene(void);
    ~EditorMapVfxScene(void) override;

    void unload(void) override;
    void load(void) override;
    void update(double dt) override;
    void draw(void) override;
    void keypressed(const char* key, SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, bool isrepeat, SDL_KeyboardID keyboardID) override;
    void mousepressed(float x, float y, RC2D_MouseButton button, int clicks, SDL_MouseID mouseID) override;
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





