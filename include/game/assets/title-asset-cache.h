#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <RC2D/RC2D.h>

/**
 * @brief Type logique d'asset traite par le preloader TITLE.
 *
 * Cette enumeration sert uniquement a reporter l'etape courante
 * du chargement initial dans la scene de loading.
 */
enum class TitleAssetKind
{
    IMAGE = 0, /**< Texture 2D sous `assets/images/`. */
    FONT,      /**< Police sous `assets/fonts/`. */
    AUDIO,     /**< Son/musique sous `assets/sounds/`. */
    VIDEO      /**< Video sous `assets/videos/`. */
};

/**
 * @brief Etat d'avancement du prechargement global des assets TITLE.
 *
 * Cette structure est lue par la scene de loading pour afficher
 * la progression du bootstrap avant l'entree dans les scenes runtime.
 */
struct TitleAssetPreloadProgress
{
    std::size_t totalEntries = 0;        /**< Nombre total d'entrees planifiees pour ce bootstrap. */
    std::size_t processedEntries = 0;    /**< Nombre d'entrees deja tentees. */
    std::size_t failedEntries = 0;       /**< Nombre d'entrees ayant echoue au chargement. */
    std::size_t loadedImageCount = 0;    /**< Nombre de textures TITLE prechargees avec succes. */
    std::size_t loadedFontCount = 0;     /**< Nombre de fichiers `.ttf` TITLE precharges avec succes. */
    std::size_t loadedAudioCount = 0;    /**< Nombre d'assets audio TITLE precharges avec succes. */
    std::size_t warmedVideoCount = 0;    /**< Nombre de videos TITLE simplement prechauffees. */
    TitleAssetKind currentKind = TitleAssetKind::IMAGE; /**< Type de l'entree actuellement traitee. */
    std::string currentPath{};           /**< Chemin de l'entree actuellement traitee. */
    bool started = false;                /**< True si un bootstrap complet a ete lance. */
    bool finished = false;               /**< True si toutes les entrees planifiees ont ete traitees. */
};

/**
 * @brief Store central des assets partageables charges depuis RC2D_STORAGE_TITLE.
 *
 * Regles de possession:
 * - le cache possede les textures/polices/audio TITLE qu'il charge;
 * - ces ressources vivent jusqu'a `clear()`/shutdown;
 * - les scenes/widgets/shaders recoivent seulement des refs/handles empruntes;
 * - ces refs empruntes ne doivent pas etre detruits dans les `unload()`.
 *
 * Les ressources non-TITLE (par exemple des imports runtime d'editeur en USER)
 * ne sont pas possedees par ce store et restent sous la responsabilite du code
 * qui les a allouees.
 */
class TitleAssetCache
{
public:
    /**
     * @brief Construit un cache vide.
     */
    TitleAssetCache();

    /**
     * @brief Destructeur.
     *
     * Par securite, appelle `clear()` afin de relacher tout ce que le cache possede.
     */
    ~TitleAssetCache();

    /**
     * @brief Detruit tous les assets possedes par le cache.
     *
     * Cette methode doit etre reservee au shutdown global du jeu
     * (ou a une reinitialisation explicite du cache).
     */
    void clear();

    /**
     * @brief Prepare un bootstrap complet des assets TITLE projet.
     *
     * La methode rescane `assets/images`, `assets/fonts`, `assets/sounds`
     * et `assets/videos`, puis reinitialise la progression de preload.
     */
    void beginFullPreload();

    /**
     * @brief Traite un batch du preload global.
     *
     * @param maxMilliseconds Budget temps max du batch courant.
     * @param maxEntries Nombre max d'entrees a traiter dans ce batch.
     * @return True si au moins une entree a ete traitee durant cet appel.
     */
    bool preloadNextBatch(double maxMilliseconds, std::size_t maxEntries);

    /**
     * @brief Retourne la progression actuelle du preload global.
     */
    const TitleAssetPreloadProgress& getPreloadProgress() const;

    /**
     * @brief Indique si le preload global est termine.
     */
    bool isPreloadFinished() const;

    /**
     * @brief Charge une image depuis un storage RC2D.
     *
     * Si le chemin pointe vers un asset TITLE partageable, la ressource
     * provient du cache global. Sinon, le chargement est fait directement
     * via RC2D et la ressource reste owned par l'appelant.
     *
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     * @return Handle image pret a etre utilise.
     */
    RC2D_Image loadImage(const char* storagePath, RC2D_StorageKind storageKind);

    /**
     * @brief Charge les donnees CPU d'une image depuis un storage RC2D.
     *
     * Pour `RC2D_STORAGE_TITLE`, les surfaces sous `assets/images/` sont
     * mutualisees par le cache. Pour les autres cas, l'appelant est owner.
     *
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     * @return Surface RC2D prete a etre lue.
     */
    RC2D_ImageData loadImageData(const char* storagePath, RC2D_StorageKind storageKind);

    /**
     * @brief Ouvre une police depuis un storage RC2D.
     *
     * Le cache mutualise les polices TITLE par couple `(path, size)`.
     *
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     * @param fontSize Taille demandee.
     * @return Police RC2D prete pour la creation de texte.
     */
    RC2D_Font openFont(const char* storagePath, RC2D_StorageKind storageKind, float fontSize);

    /**
     * @brief Charge un asset audio depuis un storage RC2D.
     *
     * Le cache mutualise les audios TITLE par couple `(path, predecode)`.
     *
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     * @param predecode True pour predecoder completement l'audio en memoire.
     * @return Pointeur audio SDL_mixer/RC2D, ou nullptr en cas d'echec.
     */
    MIX_Audio* loadAudio(const char* storagePath, RC2D_StorageKind storageKind, bool predecode);

    /**
     * @brief Ouvre une video depuis un storage RC2D.
     *
     * Les handles `RC2D_Video` restent owned par l'appelant car ils portent
     * un etat de lecture runtime. Le cache ne conserve qu'un "warmup"
     * logique pour les videos TITLE.
     *
     * @param video Handle video cible a initialiser.
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     * @return 0 en cas de succes, -1 en cas d'echec.
     */
    int openVideo(RC2D_Video* video, const char* storagePath, RC2D_StorageKind storageKind);

    /**
     * @brief Evince une texture TITLE precise du cache partage.
     *
     * Cette methode sert aux scenes qui veulent liberer une ressource
     * ponctuelle apres transition, sans detruire l'ensemble du cache.
     *
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     */
    void evictImage(const char* storagePath, RC2D_StorageKind storageKind);

    /**
     * @brief Oublie l'etat de warmup d'une video TITLE precise.
     *
     * Le handle runtime reste a fermer via `rc2d_video_close()`.
     * Cette methode ne concerne que l'entree logique conservee
     * par le cache TITLE pour les videos prechauffees.
     *
     * @param storagePath Chemin storage relatif.
     * @param storageKind Type de storage RC2D.
     */
    void evictVideo(const char* storagePath, RC2D_StorageKind storageKind);

    /**
     * @brief Relache une image owned localement.
     *
     * Si l'image pointe en realite vers un asset du cache TITLE,
     * la ressource partagee n'est pas detruite et seule la ref locale
     * est invalidee.
     *
     * @param image Handle image a relacher.
     */
    void releaseImage(RC2D_Image* image);

    /**
     * @brief Relache des donnees image owned localement.
     *
     * Si la surface appartient au cache TITLE, seule la ref locale est invalidee.
     *
     * @param imageData Surface a relacher.
     */
    void releaseImageData(RC2D_ImageData* imageData);

    /**
     * @brief Relache une police owned localement.
     *
     * Si la police appartient au cache TITLE, seule la ref locale est invalidee.
     *
     * @param font Police a relacher.
     */
    void releaseFont(RC2D_Font* font);

    /**
     * @brief Relache un asset audio owned localement.
     *
     * Si l'audio appartient au cache TITLE, il n'est pas detruit ici.
     *
     * @param audio Pointeur audio a relacher.
     */
    void releaseAudio(MIX_Audio* audio);

private:
    /**
     * @brief Cle de mutualisation d'une police TITLE.
     */
    struct FontKey
    {
        std::string path;  /**< Chemin storage normalise. */
        int fontSizeMilli = 0; /**< Taille en milli-points pour cle stable. */

        /**
         * @brief Comparateur strict total pour `std::map`.
         */
        bool operator<(const FontKey& other) const;
    };

    /**
     * @brief Cle de mutualisation d'un audio TITLE.
     */
    struct AudioKey
    {
        std::string path; /**< Chemin storage normalise. */
        bool predecode = true; /**< Variante de chargement demandee. */

        /**
         * @brief Comparateur strict total pour `std::map`.
         */
        bool operator<(const AudioKey& other) const;
    };

    /**
     * @brief Entree interne du planner de preload global.
     */
    struct PreloadEntry
    {
        TitleAssetKind kind = TitleAssetKind::IMAGE; /**< Nature de l'entree. */
        std::string path{}; /**< Chemin storage normalise. */
        float fontSize = 0.0f; /**< Taille utilisee si `kind == FONT`. */
        bool audioPredecode = true; /**< Flag utilise si `kind == AUDIO`. */
    };

    std::map<std::string, RC2D_Image> cachedImages;          /**< Textures TITLE partagees. */
    std::map<std::string, RC2D_ImageData> cachedImageData;   /**< Surfaces CPU TITLE partagees. */
    std::map<FontKey, RC2D_Font> cachedFonts;                /**< Polices TITLE partagees. */
    std::map<AudioKey, MIX_Audio*> cachedAudio;              /**< Audios TITLE partages. */
    std::vector<std::string> warmedVideos;                   /**< Liste des videos TITLE deja prechauffees. */
    std::vector<std::string> countedPreloadedFontPaths;      /**< Fichiers `.ttf` deja comptabilises dans la progression. */
    std::vector<PreloadEntry> preloadEntries;                /**< Plan courant du preload global. */
    TitleAssetPreloadProgress preloadProgress;               /**< Etat d'avancement runtime du preload. */
    std::size_t preloadIndex;                                /**< Index courant dans `preloadEntries`. */

    /**
     * @brief Normalise un chemin storage pour l'utiliser comme cle stable.
     */
    static std::string normalizeStoragePath(const char* storagePath);

    /**
     * @brief Indique si un chemin se termine par l'extension demandee.
     */
    static bool hasExtension(const std::string& path, const char* extension);

    /**
     * @brief Indique si une chaine commence par le prefixe demande.
     */
    static bool startsWith(const std::string& value, const char* prefix);

    /**
     * @brief Convertit une taille de police flottante en entier stable.
     */
    static int toFontSizeMilli(float fontSize);

    /**
     * @brief Indique si l'image doit etre mutualisee par le cache.
     */
    bool shouldCacheImage(const std::string& normalizedPath, RC2D_StorageKind storageKind) const;

    /**
     * @brief Indique si la surface CPU doit etre mutualisee par le cache.
     */
    bool shouldCacheImageData(const std::string& normalizedPath, RC2D_StorageKind storageKind) const;

    /**
     * @brief Indique si la police doit etre mutualisee par le cache.
     */
    bool shouldCacheFont(const std::string& normalizedPath, RC2D_StorageKind storageKind) const;

    /**
     * @brief Indique si l'audio doit etre mutualise par le cache.
     */
    bool shouldCacheAudio(const std::string& normalizedPath, RC2D_StorageKind storageKind) const;

    /**
     * @brief Indique si la video doit etre prechauffee par le cache.
     */
    bool shouldManageVideo(const std::string& normalizedPath, RC2D_StorageKind storageKind) const;

    /**
     * @brief Retourne une texture TITLE depuis le cache, en la chargeant si besoin.
     */
    RC2D_Image& ensureCachedImage(const std::string& normalizedPath);

    /**
     * @brief Retourne une surface CPU TITLE depuis le cache, en la chargeant si besoin.
     */
    RC2D_ImageData& ensureCachedImageData(const std::string& normalizedPath);

    /**
     * @brief Retourne une police TITLE depuis le cache, en l'ouvrant si besoin.
     */
    RC2D_Font& ensureCachedFont(const std::string& normalizedPath, float fontSize);

    /**
     * @brief Retourne un audio TITLE depuis le cache, en le chargeant si besoin.
     */
    MIX_Audio* ensureCachedAudio(const std::string& normalizedPath, bool predecode);

    /**
     * @brief Preechauffe une video TITLE si elle ne l'est pas encore.
     */
    void ensureVideoWarmed(const std::string& normalizedPath);

    /**
     * @brief Indique si une video TITLE est deja marquee comme prechauffee.
     */
    bool isVideoWarmed(const std::string& normalizedPath) const;

    /**
     * @brief Reconstruit le plan complet du preload global.
     */
    void rebuildPreloadEntries();

    /**
     * @brief Traite une seule entree du preload global.
     *
     * @param entry Entree planifiee a executer.
     * @return True si le chargement a reussi.
     */
    bool processPreloadEntry(const PreloadEntry& entry);
};

/**
 * @brief Charge une image via le cache TITLE quand c'est possible.
 *
 * @param storagePath Chemin storage relatif.
 * @param storageKind Type de storage RC2D.
 * @return Image chargee ou empruntee.
 */
RC2D_Image LoadStorageImage(const char* storagePath, RC2D_StorageKind storageKind);

/**
 * @brief Charge des donnees image via le cache TITLE quand c'est possible.
 *
 * @param storagePath Chemin storage relatif.
 * @param storageKind Type de storage RC2D.
 * @return Surface chargee ou empruntee.
 */
RC2D_ImageData LoadStorageImageData(const char* storagePath, RC2D_StorageKind storageKind);

/**
 * @brief Ouvre une police via le cache TITLE quand c'est possible.
 *
 * @param storagePath Chemin storage relatif.
 * @param storageKind Type de storage RC2D.
 * @param fontSize Taille demandee.
 * @return Police ouverte ou empruntee.
 */
RC2D_Font OpenStorageFont(const char* storagePath, RC2D_StorageKind storageKind, float fontSize);

/**
 * @brief Charge un audio via le cache TITLE quand c'est possible.
 *
 * @param storagePath Chemin storage relatif.
 * @param storageKind Type de storage RC2D.
 * @param predecode True pour predecoder l'audio.
 * @return Audio charge ou emprunte.
 */
MIX_Audio* LoadStorageAudio(const char* storagePath, RC2D_StorageKind storageKind, bool predecode);

/**
 * @brief Ouvre une video via le cache TITLE quand c'est possible.
 *
 * @param video Handle video cible a initialiser.
 * @param storagePath Chemin storage relatif.
 * @param storageKind Type de storage RC2D.
 * @return 0 en cas de succes, -1 sinon.
 */
int OpenStorageVideo(RC2D_Video* video, const char* storagePath, RC2D_StorageKind storageKind);

/**
 * @brief Invalide uniquement une ref locale vers une texture partagee.
 *
 * Cette fonction ne detruit jamais la ressource sous-jacente.
 *
 * @param image Ref locale a reinitialiser.
 */
void ResetStorageImageRef(RC2D_Image* image);

/**
 * @brief Invalide uniquement une ref locale vers des donnees image partagees.
 *
 * Cette fonction ne detruit jamais la surface sous-jacente.
 *
 * @param imageData Ref locale a reinitialiser.
 */
void ResetStorageImageDataRef(RC2D_ImageData* imageData);

/**
 * @brief Invalide uniquement une ref locale vers une police partagee.
 *
 * Cette fonction ne detruit jamais la police sous-jacente.
 *
 * @param font Ref locale a reinitialiser.
 */
void ResetStorageFontRef(RC2D_Font* font);

/**
 * @brief Invalide uniquement une ref locale vers un asset audio partage.
 *
 * Cette fonction ne detruit jamais l'asset audio sous-jacent.
 *
 * @param audio Pointeur local a reinitialiser.
 */
void ResetStorageAudioRef(MIX_Audio** audio);

/**
 * @brief Relache une image owned localement et non partagee.
 *
 * Si l'image vise un asset du cache TITLE, seule la ref locale est invalidee.
 * Cette API est reservee aux ressources temporaires/importees dont l'appelant
 * est effectivement owner.
 *
 * @param image Image a relacher.
 */
void ReleaseStorageImage(RC2D_Image* image);

/**
 * @brief Relache des donnees image owned localement et non partagees.
 *
 * Si la surface vise un asset du cache TITLE, seule la ref locale est invalidee.
 *
 * @param imageData Surface a relacher.
 */
void ReleaseStorageImageData(RC2D_ImageData* imageData);

/**
 * @brief Relache une police owned localement et non partagee.
 *
 * Si la police vise un asset du cache TITLE, seule la ref locale est invalidee.
 *
 * @param font Police a relacher.
 */
void ReleaseStorageFont(RC2D_Font* font);

/**
 * @brief Relache un audio owned localement et non partage.
 *
 * Si l'audio vise un asset du cache TITLE, il n'est pas detruit.
 *
 * @param audio Audio a relacher.
 */
void ReleaseStorageAudio(MIX_Audio* audio);
