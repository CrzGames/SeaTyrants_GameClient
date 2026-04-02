#include "network/transport/encryption/enet_host_xchacha20poly1305_encryptor.h"

#include "core/context.h"

#include <RC2D/RC2D.h>
#include <sodium.h>

#include <array>
#include <cstring>
#include <mutex>
#include <vector>

// Signature fixe en tete des payloads chiffres.
// Permet de verifier rapidement le format attendu.
static constexpr enet_uint8 kEncryptedPacketMagic[4] = {'R', 'C', 'N', '1'};

// Tailles fixes utilisees par le format du packet chiffre.
static constexpr size_t kEncryptedPacketMagicSize = sizeof(kEncryptedPacketMagic);
static constexpr size_t kEncryptedPacketNonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
static constexpr size_t kEncryptedPacketTagSize = crypto_aead_xchacha20poly1305_ietf_ABYTES;
static constexpr size_t kEncryptedPacketHeaderSize = kEncryptedPacketMagicSize + kEncryptedPacketNonceSize;

// Contexte ENet encryptor.
// Aucun etat mutable requis pour le moment.
struct ClientNetworkHostEncryptorContext
{
};

// Charge la cle TX client si le chiffrement est actif pour le peer serveur.
// Retourne false si le peer n'est pas celui de la connexion active, ou si
// le chiffrement n'est pas active.
static bool ClientNetworkEncryption_TryLoadTxKey(
    ENetPeer* peer,
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES>& outKey)
{
    if (peer == nullptr)
    {
        return false;
    }

    NetworkState& networkState = GetNetworkState();
    if (networkState.peerServer != peer)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);
    if (!networkState.secureSessionEstablished)
    {
        return false;
    }

    outKey = networkState.clientTxKey;
    return true;
}

// Charge la cle RX client si le chiffrement est actif pour le peer serveur.
// Retourne false si le peer n'est pas celui de la connexion active, ou si
// le chiffrement n'est pas active.
static bool ClientNetworkEncryption_TryLoadRxKey(
    ENetPeer* peer,
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES>& outKey)
{
    if (peer == nullptr)
    {
        return false;
    }

    NetworkState& networkState = GetNetworkState();
    if (networkState.peerServer != peer)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(networkState.sessionCryptoMutex);
    if (!networkState.secureSessionEstablished)
    {
        return false;
    }

    outKey = networkState.clientRxKey;
    return true;
}

// Linearise les buffers ENet en un bloc contigu.
// Retourne 0 si la copie est incomplète ou invalide.
static size_t ClientNetworkEncryption_CopyInBuffersToOutData(
    const ENetBuffer* inBuffers,
    size_t inBufferCount,
    size_t inLimit,
    enet_uint8* outData,
    size_t outLimit)
{
    if (inBuffers == nullptr || outData == nullptr || inLimit > outLimit)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_ENCRYPTION] [COPY] Invalid buffers (inBuffers=%p outData=%p inLimit=%zu outLimit=%zu).",
            inBuffers,
            outData,
            inLimit,
            outLimit);
        return 0;
    }

    size_t copied = 0;
    for (size_t i = 0; i < inBufferCount && copied < inLimit; ++i)
    {
        const size_t remaining = inLimit - copied;
        const size_t chunk = (inBuffers[i].dataLength < remaining) ? inBuffers[i].dataLength : remaining;

        if (chunk == 0)
        {
            continue;
        }

        if (inBuffers[i].data == nullptr)
        {
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [NETWORK_ENCRYPTION] [COPY] Null data pointer at fragment=%u.",
                static_cast<unsigned>(i));
            return 0;
        }

        std::memcpy(outData + copied, inBuffers[i].data, chunk);
        copied += chunk;
    }

    if (copied != inLimit)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_ENCRYPTION] [COPY] Incomplete copy (copied=%zu expected=%zu).",
            copied,
            inLimit);
        return 0;
    }

    return copied;
}

// Callback ENet appele avant envoi.
// Retour 0 => ENet envoie le packet en clair (non chiffre).
static size_t ENET_CALLBACK ClientNetworkEncryption_EncryptCallback(
    void* /*context*/,
    ENetPeer* peer,
    const ENetBuffer* inBuffers,
    size_t inBufferCount,
    size_t inLimit,
    enet_uint8* outData,
    size_t outLimit)
{
    // Charger la cle TX seulement si le chiffrement est actif pour ce peer.
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> txKey{};
    if (!ClientNetworkEncryption_TryLoadTxKey(peer, txKey))
    {
        return 0;
    }

    // Verifier l'espace de sortie necessaire:
    // [magic + nonce] + [ciphertext + tag].
    const size_t requiredSize = kEncryptedPacketHeaderSize + inLimit + kEncryptedPacketTagSize;
    if (requiredSize > outLimit)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [NETWORK_ENCRYPTION] [ENCRYPT] Output buffer too small (required=%zu outLimit=%zu).",
            requiredSize,
            outLimit);
        sodium_memzero(txKey.data(), txKey.size());
        return 0;
    }

    // Convertir les fragments ENet en plaintext contigu.
    std::vector<enet_uint8> plaintext(inLimit);
    if (ClientNetworkEncryption_CopyInBuffersToOutData(
            inBuffers,
            inBufferCount,
            inLimit,
            plaintext.data(),
            plaintext.size()) != inLimit)
    {
        sodium_memzero(txKey.data(), txKey.size());
        sodium_memzero(plaintext.data(), plaintext.size());
        return 0;
    }

    // Ecrire le magic en tete du packet chiffre.
    std::memcpy(outData, kEncryptedPacketMagic, kEncryptedPacketMagicSize);

    // Generer un nonce unique pour ce packet.
    enet_uint8* nonce = outData + kEncryptedPacketMagicSize;
    randombytes_buf(nonce, kEncryptedPacketNonceSize);

    // Chiffrer avec XChaCha20-Poly1305.
    unsigned long long ciphertextLen = 0;
    const int encryptResult = crypto_aead_xchacha20poly1305_ietf_encrypt(
        outData + kEncryptedPacketHeaderSize,
        &ciphertextLen,
        plaintext.data(),
        plaintext.size(),
        nullptr,
        0,
        nullptr,
        nonce,
        txKey.data());

    // Effacer les donnees sensibles.
    sodium_memzero(txKey.data(), txKey.size());
    sodium_memzero(plaintext.data(), plaintext.size());

    if (encryptResult != 0)
    {
        RC2D_log(RC2D_LOG_ERROR, "[CLIENT] [NETWORK_ENCRYPTION] [ENCRYPT] libsodium encrypt failed.");
        return 0;
    }

    // Retourner la taille totale produite.
    return kEncryptedPacketHeaderSize + static_cast<size_t>(ciphertextLen);
}

// Callback ENet appele pour dechiffrer un packet marque comme chiffre.
// Retour 0 => paquet rejete.
static size_t ENET_CALLBACK ClientNetworkEncryption_DecryptCallback(
    void* /*context*/,
    ENetPeer* peer,
    const enet_uint8* inData,
    size_t inLimit,
    enet_uint8* outData,
    size_t outLimit)
{
    // Charger la cle RX seulement si le chiffrement est actif pour ce peer.
    std::array<uint8_t, crypto_kx_SESSIONKEYBYTES> rxKey{};
    if (!ClientNetworkEncryption_TryLoadRxKey(peer, rxKey))
    {
        return 0;
    }

    // Verifier format minimal: [magic + nonce] + [tag].
    if (inData == nullptr || inLimit < (kEncryptedPacketHeaderSize + kEncryptedPacketTagSize))
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_ENCRYPTION] [DECRYPT] Invalid input packet (inData=%p inLimit=%zu).",
            inData,
            inLimit);
        sodium_memzero(rxKey.data(), rxKey.size());
        return 0;
    }

    // Verifier la signature de format.
    if (std::memcmp(inData, kEncryptedPacketMagic, kEncryptedPacketMagicSize) != 0)
    {
        RC2D_log(RC2D_LOG_WARN, "[CLIENT] [NETWORK_ENCRYPTION] [DECRYPT] Invalid packet magic.");
        sodium_memzero(rxKey.data(), rxKey.size());
        return 0;
    }

    // Extraire nonce + ciphertext.
    const enet_uint8* nonce = inData + kEncryptedPacketMagicSize;
    const enet_uint8* ciphertext = inData + kEncryptedPacketHeaderSize;
    const size_t ciphertextLen = inLimit - kEncryptedPacketHeaderSize;

    // Taille plaintext maximale theorique.
    const size_t maximumPlaintextLen = ciphertextLen - kEncryptedPacketTagSize;
    if (maximumPlaintextLen > outLimit)
    {
        RC2D_log(
            RC2D_LOG_WARN,
            "[CLIENT] [NETWORK_ENCRYPTION] [DECRYPT] Output buffer too small (maxPlaintext=%zu outLimit=%zu).",
            maximumPlaintextLen,
            outLimit);
        sodium_memzero(rxKey.data(), rxKey.size());
        return 0;
    }

    // Dechiffrer et verifier l'authenticite AEAD.
    unsigned long long plaintextLen = 0;
    const int decryptResult = crypto_aead_xchacha20poly1305_ietf_decrypt(
        outData,
        &plaintextLen,
        nullptr,
        ciphertext,
        ciphertextLen,
        nullptr,
        0,
        nonce,
        rxKey.data());

    // Effacer la cle sensible.
    sodium_memzero(rxKey.data(), rxKey.size());

    if (decryptResult != 0)
    {
        RC2D_log(RC2D_LOG_WARN, "[CLIENT] [NETWORK_ENCRYPTION] [DECRYPT] Authentication failed.");
        return 0;
    }

    // Retourner la taille plaintext produite.
    return static_cast<size_t>(plaintextLen);
}

void ClientNetworkEncryption_EnsureHostEncryptorInstalled(ENetHost* host)
{
    // Protection de base sur pointeur host.
    if (host == nullptr)
    {
        RC2D_log(RC2D_LOG_ERROR, "[CLIENT] [NETWORK_ENCRYPTION] host is null.");
        return;
    }

    // Instance statique unique de l'encryptor pour eviter reallocation/rebind.
    static ClientNetworkHostEncryptorContext encryptorContext{};
    static ENetEncryptor encryptor = {
        &encryptorContext,
        ClientNetworkEncryption_EncryptCallback,
        ClientNetworkEncryption_DecryptCallback,
        nullptr};

    // Idempotence: si deja installe, ne rien faire.
    if (host->encryptor.context == encryptor.context &&
        host->encryptor.encrypt == encryptor.encrypt &&
        host->encryptor.decrypt == encryptor.decrypt)
    {
        return;
    }

    // Installer l'encryptor host-level ENet6.
    enet_host_encrypt(host, &encryptor);
    RC2D_log(RC2D_LOG_INFO, "[CLIENT] [NETWORK_ENCRYPTION] ENet host encryptor installed.");
}

