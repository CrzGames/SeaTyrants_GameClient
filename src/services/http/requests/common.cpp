#include "services/http/requests/common.h"

#include "services/http/tls/ca_bundle_pem.h"

#include <cJSON.h>

// Retourne true si l'URL est en HTTPS.
static bool ClientHttp_Common_IsHttpsUrl(const std::string& url)
{
    return url.rfind("https://", 0) == 0;
}

size_t ClientHttp_Common_WriteResponseBodyCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userData)
{
    // Securite: sans buffer source ou destination, impossible de copier.
    if (contents == nullptr || userData == nullptr)
    {
        // 0 indique a libcurl qu'aucun octet n'a ete consomme.
        return 0;
    }

    // Taille reelle du chunk recu (en bytes).
    const size_t byteCount = size * nmemb;

    // S'il n'y a rien a copier, on sort proprement.
    if (byteCount == 0)
    {
        return 0;
    }

    // Reinterprete le pointeur utilisateur en std::string*.
    std::string* outBody = static_cast<std::string*>(userData);

    // Concatene le chunk recu a la fin du body HTTP accumule.
    outBody->append(static_cast<const char*>(contents), byteCount);

    // Retourne le nombre d'octets consommes pour confirmer a libcurl.
    return byteCount;
}

bool ClientHttp_Common_ReadRequiredString(
    const cJSON* jsonObject,
    const char* key,
    std::string& outValue)
{
    // Sans objet source ni nom de champ, lecture impossible.
    if (jsonObject == nullptr || key == nullptr)
    {
        return false;
    }

    // Chercher le champ cible dans l'objet.
    const cJSON* valueItem = cJSON_GetObjectItemCaseSensitive(jsonObject, key);
    if (!cJSON_IsString(valueItem) || valueItem->valuestring == nullptr)
    {
        // Echec si le champ est absent ou n'est pas une string valide.
        return false;
    }

    // Copier la valeur string en sortie.
    outValue = valueItem->valuestring;
    return true;
}

bool ClientHttp_Common_ConfigureTlsForHttps(
    CURL* curl,
    const std::string& url,
    std::string& outError)
{
    if (curl == nullptr || !ClientHttp_Common_IsHttpsUrl(url))
    {
        return true;
    }

    // Exiger la verification TLS du certificat serveur.
    CURLcode curlCode = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    if (curlCode != CURLE_OK)
    {
        outError = std::string("Failed to set CURLOPT_SSL_VERIFYPEER: ") + curl_easy_strerror(curlCode);
        return false;
    }

    curlCode = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (curlCode != CURLE_OK)
    {
        outError = std::string("Failed to set CURLOPT_SSL_VERIFYHOST: ") + curl_easy_strerror(curlCode);
        return false;
    }

#if defined(LIBCURL_VERSION_NUM) && (LIBCURL_VERSION_NUM >= 0x074D00)
    curl_blob caBundleBlob{};
    caBundleBlob.data = const_cast<char*>(kClientHttpTlsCaBundlePem);
    caBundleBlob.len = kClientHttpTlsCaBundlePemSize;
    caBundleBlob.flags = CURL_BLOB_NOCOPY;

    curlCode = curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &caBundleBlob);
    if (curlCode != CURLE_OK)
    {
        outError = std::string("Failed to set CURLOPT_CAINFO_BLOB: ") + curl_easy_strerror(curlCode);
        return false;
    }

    return true;
#else
    outError = "This libcurl build does not support CAINFO_BLOB (requires libcurl >= 7.77.0).";
    return false;
#endif
}
