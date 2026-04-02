#include "services/http/requests/auth_signinrequest.h"

#include "core/context.h"
#include "services/http/requests/common.h"

#include <RC2D/RC2D.h>
#include <curl/curl.h>
#include <cJSON.h>

#include <limits>  // std::numeric_limits
#include <string>  // std::string

AuthSignInHTTPResponse ClientHttp_Auth_SignInRequest(const AuthSignInHTTPRequest& request)
{
    // Reponse renvoyee au thread HTTP appelant.
    AuthSignInHTTPResponse response{};

    // Valeur defensive par defaut: echec tant que non prouve.
    response.success = false;

    // Recupere l'etat reseau global (contient baseUrlApi selon environnement).
    const NetworkState& networkState = GetNetworkState();

    // Construit l'URL complete du endpoint signin.
    const std::string url = std::string(networkState.baseUrlApi) + "/auth/sign-in";

    // Cree un objet JSON vide pour le payload POST.
    cJSON* jsonBody = cJSON_CreateObject();

    // Si allocation JSON echoue, retourner une erreur claire.
    if (jsonBody == nullptr)
    {
        response.message = "Failed to allocate signin JSON body.";
        return response;
    }

    // Ajoute l'email du joueur dans le JSON.
    cJSON_AddStringToObject(jsonBody, "email", request.email.c_str());

    // Ajoute le mot de passe du joueur dans le JSON.
    cJSON_AddStringToObject(jsonBody, "password", request.password.c_str());

    // Serialize l'objet JSON en texte compact.
    char* jsonBodyText = cJSON_PrintUnformatted(jsonBody);

    // Libere l'objet JSON cJSON (le texte serialize est deja alloue a part).
    cJSON_Delete(jsonBody);

    // Si la serialisation JSON echoue, retourner une erreur claire.
    if (jsonBodyText == nullptr)
    {
        response.message = "Failed to serialize signin JSON body.";
        return response;
    }

    // Copie le JSON serialize dans un std::string C++.
    std::string requestBody = jsonBodyText;

    // Libere le buffer C alloue par cJSON_PrintUnformatted.
    cJSON_free(jsonBodyText);

    // Verifie que la taille est representable en long pour CURLOPT_POSTFIELDSIZE.
    if (requestBody.size() > static_cast<size_t>((std::numeric_limits<long>::max)()))
    {
        response.message = "Signin request body is too large.";
        return response;
    }

    // Initialise un handle libcurl pour cette requete.
    CURL* curl = curl_easy_init();

    // Si init libcurl echoue, retourner une erreur claire.
    if (curl == nullptr)
    {
        response.message = "Failed to initialize CURL for signin request.";
        return response;
    }

    // Buffer qui accumule le body de reponse HTTP.
    std::string responseBody;
    char curlErrorBuffer[CURL_ERROR_SIZE] = {0};

    // Liste de headers HTTP a envoyer.
    curl_slist* headers = nullptr;

    // Header JSON pour indiquer le type de body envoye.
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Header JSON pour indiquer le type de body attendu en retour.
    headers = curl_slist_append(headers, "Accept: application/json");

    // Configure l'URL cible.
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Buffer detaille d'erreurs cURL (TLS, DNS, socket...).
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuffer);

    // Force la methode HTTP POST.
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Applique les headers HTTP.
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Fournit le body POST (JSON texte).
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());

    // Fournit la taille explicite du body POST.
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));

    // Enregistre le callback de reception du body HTTP.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ClientHttp_Common_WriteResponseBodyCallback);

    // Passe la destination de sortie au callback.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    // Evite les signaux POSIX (utile en contexte multithread).
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // Timeout de connexion (ms).
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);

    // Timeout global de requete (ms).
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);

    // Configuration TLS pour les URLs HTTPS.
    std::string tlsConfigurationError;
    if (!ClientHttp_Common_ConfigureTlsForHttps(curl, url, tlsConfigurationError))
    {
        response.success = false;
        response.message = std::string("Signin TLS configuration failed: ") + tlsConfigurationError;

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] %s",
            response.message.c_str());
        return response;
    }

    // Log de debug/trace pour savoir quel endpoint est appele.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [HTTP] [AUTH_SIGNIN] POST %s",
        url.c_str());

    // Execute la requete HTTP.
    const CURLcode curlCode = curl_easy_perform(curl);

    // Variable pour stocker le code HTTP (200, 401, etc.).
    long httpStatusCode = 0;

    // Recupere le code status HTTP renvoye par le serveur.
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatusCode);

    // Libere la liste des headers.
    curl_slist_free_all(headers);

    // Libere le handle CURL.
    curl_easy_cleanup(curl);

    // Echec niveau transport (DNS/TLS/socket/timeout...).
    if (curlCode != CURLE_OK)
    {
        // Reponse metier: echec.
        response.success = false;

        // Message detaille base sur le buffer d'erreur cURL si disponible.
        const char* detailedError = (curlErrorBuffer[0] != '\0')
            ? curlErrorBuffer
            : curl_easy_strerror(curlCode);
        response.message = std::string("Signin request failed: ") + detailedError;

        // Log technique pour diagnostic.
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] CURL error=%d message=%s",
            static_cast<int>(curlCode),
            response.message.c_str());

        // Retour immediat en cas d'erreur transport.
        return response;
    }

    // Memorise le status HTTP dans la reponse metier.
    response.httpStatusCode = httpStatusCode;

    // Parse JSON uniquement si le body n'est pas vide.
    cJSON* responseJson = nullptr;
    if (!responseBody.empty())
    {
        responseJson = cJSON_Parse(responseBody.c_str());
    }

    // ---------------------------------------------------------------------
    // Cas succes attendu: HTTP 200
    // Payload:
    // {
    //   "token": { "type": "...", "value": "...", "expiresAt": "..." },
    //   "quilkin_dns": "...",
    //   "quilkin_port": 7777
    // }
    // ---------------------------------------------------------------------
    if (httpStatusCode == 200)
    {
        // Sans JSON valide, payload invalide.
        if (responseJson == nullptr)
        {
            response.success = false;
            response.message = "Sign-in succeeded with invalid JSON payload.";
        }
        else
        {
            // Extraire l'objet token.
            const cJSON* tokenItem = cJSON_GetObjectItemCaseSensitive(responseJson, "token");
            const cJSON* quilkinPortItem = cJSON_GetObjectItemCaseSensitive(responseJson, "quilkin_port");

            // Lire les champs string obligatoires.
            const bool tokenTypeOk = cJSON_IsObject(tokenItem) &&
                ClientHttp_Common_ReadRequiredString(tokenItem, "type", response.tokenType);
            const bool tokenValueOk = cJSON_IsObject(tokenItem) &&
                ClientHttp_Common_ReadRequiredString(tokenItem, "value", response.tokenValue);
            const bool tokenExpiresAtOk = cJSON_IsObject(tokenItem) &&
                ClientHttp_Common_ReadRequiredString(tokenItem, "expiresAt", response.tokenExpiresAt);
            const bool quilkinDnsOk =
                ClientHttp_Common_ReadRequiredString(responseJson, "quilkin_dns", response.quilkinDns);

            // Lire quilkin_port (doit etre un entier valide dans [1..65535]).
            bool quilkinPortOk = false;
            if (cJSON_IsNumber(quilkinPortItem))
            {
                const int port = quilkinPortItem->valueint;
                if (port > 0 && port <= 65535)
                {
                    response.quilkinPort = static_cast<uint16_t>(port);
                    quilkinPortOk = true;
                }
            }

            // Valider completude du payload succes.
            response.success = tokenTypeOk && tokenValueOk && tokenExpiresAtOk && quilkinDnsOk && quilkinPortOk;

            if (response.success)
            {
                response.message = "Sign-in succeeded.";
            }
            else
            {
                response.message = "Sign-in response payload is missing required fields.";
            }
        }

        // Nettoyer l'arbre JSON parse.
        if (responseJson != nullptr)
        {
            cJSON_Delete(responseJson);
        }

        RC2D_log(
            response.success ? RC2D_LOG_INFO : RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] HTTP status=%ld success=%u message=%s",
            response.httpStatusCode,
            response.success ? 1u : 0u,
            response.message.c_str());
        return response;
    }

    // ---------------------------------------------------------------------
    // Cas echec (ex: HTTP 500)
    // Payload attendu:
    // {
    //   "code": "E_INTERNAL_SERVER_ERROR",
    //   "message": "Failed to authenticate"
    // }
    // ---------------------------------------------------------------------
    response.success = false;

    if (responseJson != nullptr)
    {
        // Lire les champs d'erreur quand presents.
        ClientHttp_Common_ReadRequiredString(responseJson, "code", response.code);
        ClientHttp_Common_ReadRequiredString(responseJson, "message", response.message);

        // Nettoyer l'arbre JSON parse.
        cJSON_Delete(responseJson);
    }

    // Message fallback si le backend ne fournit pas de message.
    if (response.message.empty())
    {
        response.message = std::string("Sign-in failed (HTTP ") + std::to_string(httpStatusCode) + ").";
    }

    // Log final de resultat metier.
    RC2D_log(
        RC2D_LOG_WARN,
        "[CLIENT] [HTTP] [AUTH_SIGNIN] HTTP status=%ld success=%u code=%s message=%s",
        response.httpStatusCode,
        response.success ? 1u : 0u,
        response.code.empty() ? "<none>" : response.code.c_str(),
        response.message.c_str());

    // Retourne la reponse finale au dispatcher HTTP.
    return response;
}
