#include "network/protocol/secure_session.h"

#include <RC2D/RC2D.h>

#include <cstring> // std::strlen

static bool gClientSecureSessionPinnedServerPublicKeyInitialized = false;
static std::array<uint8_t, crypto_sign_PUBLICKEYBYTES> gClientSecureSessionPinnedServerPublicKey{};

static bool ClientSecureSession_TryDecodeHexNibble(char c, uint8_t& outValue)
{
    if (c >= '0' && c <= '9')
    {
        outValue = static_cast<uint8_t>(c - '0');
        return true;
    }

    if (c >= 'a' && c <= 'f')
    {
        outValue = static_cast<uint8_t>(10 + (c - 'a'));
        return true;
    }

    if (c >= 'A' && c <= 'F')
    {
        outValue = static_cast<uint8_t>(10 + (c - 'A'));
        return true;
    }

    return false;
}

static bool ClientSecureSession_TryResolvePinnedServerSigningPublicKeyHex(
    const char*& outHexKey,
    const char*& outSourceLabel)
{
#if GAME_ENV_DEV
    outHexKey = CLIENT_SECURE_SESSION_SERVER_ED25519_PUBLIC_KEY_HEX_DEV;
    outSourceLabel = "hardcoded-dev";
    return true;
#elif GAME_ENV_STAGING
    outHexKey = CLIENT_SECURE_SESSION_SERVER_ED25519_PUBLIC_KEY_HEX_STAGING;
    outSourceLabel = "hardcoded-staging";
    return true;
#elif GAME_ENV_PRODUCTION
    outHexKey = CLIENT_SECURE_SESSION_SERVER_ED25519_PUBLIC_KEY_HEX_PRODUCTION;
    outSourceLabel = "hardcoded-production";
    return true;
#else
    outHexKey = nullptr;
    outSourceLabel = "unknown";
    RC2D_log(
        RC2D_LOG_ERROR,
        "[CLIENT] [SECURE_SESSION] Invalid GAME_ENV compile-time configuration.");
    return false;
#endif
}

bool ClientSecureSession_InitializePinnedServerSigningPublicKey()
{
    if (gClientSecureSessionPinnedServerPublicKeyInitialized)
    {
        return true;
    }

    const char* selectedHexKey = nullptr;
    const char* selectedSourceLabel = nullptr;
    if (!ClientSecureSession_TryResolvePinnedServerSigningPublicKeyHex(selectedHexKey, selectedSourceLabel))
    {
        return false;
    }

    constexpr size_t kExpectedHexLength = crypto_sign_PUBLICKEYBYTES * 2;
    const size_t actualHexLength = std::strlen(selectedHexKey);
    if (actualHexLength != kExpectedHexLength)
    {
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [SECURE_SESSION] Invalid pinned Ed25519 public key hex length (source=%s actual=%u expected=%u).",
            selectedSourceLabel,
            static_cast<unsigned>(actualHexLength),
            static_cast<unsigned>(kExpectedHexLength));
        return false;
    }

    for (size_t i = 0; i < crypto_sign_PUBLICKEYBYTES; ++i)
    {
        const char hiChar = selectedHexKey[i * 2];
        const char loChar = selectedHexKey[i * 2 + 1];

        uint8_t hiNibble = 0;
        uint8_t loNibble = 0;
        if (!ClientSecureSession_TryDecodeHexNibble(hiChar, hiNibble) ||
            !ClientSecureSession_TryDecodeHexNibble(loChar, loNibble))
        {
            RC2D_log(
                RC2D_LOG_ERROR,
                "[CLIENT] [SECURE_SESSION] Invalid pinned Ed25519 public key hex (source=%s index=%u).",
                selectedSourceLabel,
                static_cast<unsigned>(i));
            return false;
        }

        gClientSecureSessionPinnedServerPublicKey[i] =
            static_cast<uint8_t>((hiNibble << 4) | loNibble);
    }

    gClientSecureSessionPinnedServerPublicKeyInitialized = true;

    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [SECURE_SESSION] Pinned server Ed25519 public key initialized (source=%s).",
        selectedSourceLabel);
    return true;
}

bool ClientSecureSession_GetPinnedServerSigningPublicKey(
    std::array<uint8_t, crypto_sign_PUBLICKEYBYTES>& outPublicKey)
{
    if (!gClientSecureSessionPinnedServerPublicKeyInitialized)
    {
        return false;
    }

    outPublicKey = gClientSecureSessionPinnedServerPublicKey;
    return true;
}
