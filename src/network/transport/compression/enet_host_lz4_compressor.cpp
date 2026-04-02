#include "network/transport/compression/enet_host_lz4_compressor.h"

#include <cstring> // std::memcpy
#include <limits>  // std::numeric_limits
#include <vector>  // std::vector

#include <lz4/lz4.h>     // LZ4_decompress_safe
#include <lz4/lz4hc.h>   // LZ4_compress_HC, LZ4HC_CLEVEL_MAX
#include <RC2D/RC2D.h> // RC2D_log

// Contexte du compresseur LZ4.
// Pour l'instant nous n'avons pas d'etat mutable a conserver.
struct ClientNetworkHostLz4CompressorContext
{
};

// Copie les inBuffers ENet vers un buffer contigu.
// L'API LZ4 travaille sur des blocs contigus (src/dst), donc on linearise.
static size_t ClientNetworkCompression_CopyInBuffersToContiguous(
    const ENetBuffer* inBuffers,
    size_t inBufferCount,
    size_t inLimit,
    std::vector<enet_uint8>& outContiguous)
{
    // Validation de base : sans pointeur d'entree, impossible de copier.
    if (inBuffers == nullptr)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] [COPY] - inBuffers is null");
        return 0;
    }

    // Allouer/initialiser le buffer de sortie a la taille attendue.
    outContiguous.assign(inLimit, 0);

    // Compteur d'octets effectivement copies.
    size_t copied = 0;

    // Parcourir chaque fragment ENet jusqu'a atteindre inLimit.
    for (size_t i = 0; i < inBufferCount && copied < inLimit; ++i)
    {
        // Nombre d'octets restants a recopier.
        const size_t remaining = inLimit - copied;

        // Taille du fragment utile pour cette iteration.
        const size_t chunk =
            (inBuffers[i].dataLength < remaining) ? inBuffers[i].dataLength : remaining;

        // Si le fragment est vide, passer au suivant.
        if (chunk == 0)
        {
            continue;
        }

        // Si le pointeur de donnees du fragment est invalide, echec.
        if (inBuffers[i].data == nullptr)
        {
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [NETWORK_COMPRESSION] [COPY] - inBuffers[%u].data is null",
                static_cast<unsigned>(i));
            return 0;
        }

        // Copier le fragment dans le buffer contigu.
        std::memcpy(outContiguous.data() + copied, inBuffers[i].data, chunk);
        copied += chunk;
    }

    // Retourner le nombre total d'octets copies.
    return copied;
}

// Callback ENet appele pour tenter de compresser un datagramme sortant.
static size_t ENET_CALLBACK ClientNetworkCompression_Lz4CompressCallback(
    void* /*context*/,
    const ENetBuffer* inBuffers,
    size_t inBufferCount,
    size_t inLimit,
    enet_uint8* outData,
    size_t outLimit)
{
    // Verification des pointeurs obligatoires.
    if (inBuffers == nullptr || outData == nullptr)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] [COMPRESS] - invalid pointers (inBuffers=%p outData=%p)",
            inBuffers,
            outData);
        return 0;
    }

    // Cas invalides : rien a compresser, ou sortie vide.
    if (inLimit == 0 || outLimit == 0)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_COMPRESSION] [COMPRESS] - invalid sizes (inLimit=%zu outLimit=%zu)",
            inLimit,
            outLimit);
        return 0;
    }

    // ENet ne garde la compression que si la sortie est strictement plus petite.
    // Si le buffer de sortie est plus petit que l'entree, on skip silencieusement.
    if (inLimit > outLimit)
    {
        return 0;
    }

    // Les fonctions LZ4 prennent des int pour les tailles.
    // On protege la conversion size_t -> int.
    if (inLimit > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
        outLimit > static_cast<size_t>((std::numeric_limits<int>::max)()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] [COMPRESS] - size overflow for LZ4 int API (inLimit=%zu outLimit=%zu)",
            inLimit,
            outLimit);
        return 0;
    }

    // Lineariser les fragments ENet pour fournir un bloc source contigu a LZ4.
    std::vector<enet_uint8> input;
    const size_t copied = ClientNetworkCompression_CopyInBuffersToContiguous(
        inBuffers,
        inBufferCount,
        inLimit,
        input);
    if (copied != inLimit)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] [COMPRESS] - failed to linearize buffers (copied=%zu expected=%zu)",
            copied,
            inLimit);
        return 0;
    }

    // Mode "ratio maximum" avec LZ4HC:
    // - compression plus couteuse CPU
    // - meilleur ratio que LZ4 classique
    // Si outLimit est trop petit, LZ4HC renvoie 0 et ENet enverra en clair non compresse.
    const int compressedSize = LZ4_compress_HC(
        reinterpret_cast<const char*>(input.data()),
        reinterpret_cast<char*>(outData),
        static_cast<int>(inLimit),
        static_cast<int>(outLimit),
        LZ4HC_CLEVEL_MAX);

    // Retour <= 0: echec de compression dans le budget de sortie.
    if (compressedSize <= 0)
    {
        return 0;
    }

    // Si la compression n'apporte pas de gain de taille, on refuse.
    // ENet enverra alors le paquet non compresse.
    if (static_cast<size_t>(compressedSize) >= inLimit)
    {
        return 0;
    }

    // Retourner la taille compressee produite.
    return static_cast<size_t>(compressedSize);
}

// Callback ENet appele pour decompresser un datagramme entrant marque comme compresse.
static size_t ENET_CALLBACK ClientNetworkCompression_Lz4DecompressCallback(
    void* /*context*/,
    const enet_uint8* inData,
    size_t inLimit,
    enet_uint8* outData,
    size_t outLimit)
{
    // Verification des pointeurs obligatoires.
    if (inData == nullptr || outData == nullptr)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] [DECOMPRESS] - invalid pointers (inData=%p outData=%p)",
            inData,
            outData);
        return 0;
    }

    // Cas invalides : pas d'entree ou pas d'espace de sortie.
    if (inLimit == 0 || outLimit == 0)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_COMPRESSION] [DECOMPRESS] - invalid sizes (inLimit=%zu outLimit=%zu)",
            inLimit,
            outLimit);
        return 0;
    }

    // Les fonctions LZ4 prennent des int pour les tailles.
    // On protege la conversion size_t -> int.
    if (inLimit > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
        outLimit > static_cast<size_t>((std::numeric_limits<int>::max)()))
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] [DECOMPRESS] - size overflow for LZ4 int API (inLimit=%zu outLimit=%zu)",
            inLimit,
            outLimit);
        return 0;
    }

    // Decompression LZ4 securisee :
    // - retourne < 0 en cas d'erreur (donnees invalides/corrompues).
    // - retourne >= 0 : taille de donnees decompressees.
    const int decompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(inData),
        reinterpret_cast<char*>(outData),
        static_cast<int>(inLimit),
        static_cast<int>(outLimit));

    if (decompressedSize < 0)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_COMPRESSION] [DECOMPRESS] - LZ4_decompress_safe failed (inLimit=%zu outLimit=%zu)",
            inLimit,
            outLimit);
        return 0;
    }

    // Retourner la taille de sortie decompressee.
    return static_cast<size_t>(decompressedSize);
}
void ClientNetworkCompression_EnsureHostCompressorInstalled(ENetHost* host)
{
    // Guard : host invalide => rien a faire.
    if (host == nullptr)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_COMPRESSION] - host is null, compressor install skipped");
        return;
    }

    // Instances statiques partagees pour eviter des reallocations/reinstallations inutiles.
    static ClientNetworkHostLz4CompressorContext compressorContext{};
    static ENetCompressor compressor = {
        &compressorContext,
        ClientNetworkCompression_Lz4CompressCallback,
        ClientNetworkCompression_Lz4DecompressCallback,
        nullptr // pas de callback destroy necessaire ici
    };

    // Idempotence : si c'est deja ce compresseur qui est branche, sortir.
    if (host->compressor.context == compressor.context &&
        host->compressor.compress == compressor.compress &&
        host->compressor.decompress == compressor.decompress)
    {
        return;
    }

    // Brancher le compresseur au host ENet.
    enet_host_compress(host, &compressor);

    // Log d'information pour faciliter le diagnostic runtime.
    RC2D_log(RC2D_LOG_INFO, "[CLIENT] [NETWORK_COMPRESSION] - ENet host LZ4 compressor installed");
}
