#include "services/http/requests/auth_signinrequest.h"

#include "core/context.h"
#include "services/http/requests/common.h"

#include <RC2D/RC2D.h>
#include <cJSON.h>
#include <curl/curl.h>

#include <limits>  // std::numeric_limits
#include <string>  // std::string
#include <utility> // std::move

// Appliquer un code/message client coherents pour un echec purement local.
static void ClientHttp_Auth_SignInRequest_SetClientFailure(
    const char* code,
    const char* message,
    AuthSignInHTTPResponse& response)
{
    response.success = false;
    response.code = code != nullptr ? code : "E_BACKEND_CLIENT";
    response.message = message != nullptr ? message : "Erreur interne lors de la connexion au backend SeaTyrants.";
}

// Construire un message client lisible selon l'erreur de transport libcurl.
static void ClientHttp_Auth_SignInRequest_MapTransportFailure(
    const CURLcode curlCode,
    AuthSignInHTTPResponse& response)
{
    switch (curlCode)
    {
        case CURLE_COULDNT_CONNECT:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_GOT_NOTHING:
            response.code = "E_BACKEND_UNAVAILABLE";
            response.message = "Serveur web SeaTyrants indisponible.";
            return;

        case CURLE_OPERATION_TIMEDOUT:
            response.code = "E_BACKEND_TIMEOUT";
            response.message = "Le serveur web SeaTyrants met trop de temps a repondre.";
            return;

        case CURLE_URL_MALFORMAT:
        case CURLE_UNSUPPORTED_PROTOCOL:
            response.code = "E_BACKEND_URL";
            response.message = "Configuration invalide de l'URL du backend SeaTyrants.";
            return;

        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CACERT_BADFILE:
            response.code = "E_BACKEND_TLS";
            response.message = "Connexion securisee au backend impossible.";
            return;

        default:
            response.code = "E_BACKEND_NETWORK";
            response.message = "Erreur reseau lors de la connexion au backend SeaTyrants.";
            return;
    }
}

// Extraire si possible un premier message de validation Adonis/Vine.
static bool ClientHttp_Auth_SignInRequest_ReadValidationMessage(
    const cJSON* jsonObject,
    std::string& outMessage)
{
    if (jsonObject == nullptr)
    {
        return false;
    }

    const cJSON* errorsItem = cJSON_GetObjectItemCaseSensitive(jsonObject, "errors");
    if (!cJSON_IsArray(errorsItem))
    {
        return false;
    }

    const cJSON* firstError = cJSON_GetArrayItem(errorsItem, 0);
    if (!cJSON_IsObject(firstError))
    {
        return false;
    }

    return ClientHttp_Common_ReadRequiredString(firstError, "message", outMessage);
}

// Traduire les erreurs de validation backend les plus frequentes en francais.
static std::string ClientHttp_Auth_SignInRequest_TranslateValidationMessage(
    const cJSON* jsonObject,
    const std::string& backendMessage)
{
    if (jsonObject == nullptr)
    {
        return backendMessage;
    }

    const cJSON* errorsItem = cJSON_GetObjectItemCaseSensitive(jsonObject, "errors");
    if (!cJSON_IsArray(errorsItem))
    {
        return backendMessage;
    }

    const cJSON* firstError = cJSON_GetArrayItem(errorsItem, 0);
    if (!cJSON_IsObject(firstError))
    {
        return backendMessage;
    }

    std::string field{};
    std::string rule{};
    ClientHttp_Common_ReadRequiredString(firstError, "field", field);
    ClientHttp_Common_ReadRequiredString(firstError, "rule", rule);

    if (field == "email" && rule == "email")
    {
        return "L'adresse e-mail n'est pas dans un format valide.";
    }

    if (field == "email" && rule == "required")
    {
        return "L'adresse e-mail est requise.";
    }

    if ((field == "password" || field == "motDePasse") && rule == "required")
    {
        return "Le mot de passe est requis.";
    }

    if (backendMessage.find("valid email") != std::string::npos ||
        backendMessage.find("email address") != std::string::npos)
    {
        return "L'adresse e-mail n'est pas dans un format valide.";
    }

    return backendMessage;
}

// Remplacer les messages bruts backend par des formulations joueur plus claires.
static void ClientHttp_Auth_SignInRequest_NormalizeHttpFailureMessage(
    AuthSignInHTTPResponse& response,
    const cJSON* responseJson)
{
    if (response.httpStatusCode == 401)
    {
        response.code = response.code.empty() ? "E_UNAUTHORIZED" : response.code;
        response.message = "Identifiants incorrects.";
        return;
    }

    if (response.httpStatusCode == 403)
    {
        response.code = response.code.empty() ? "E_FORBIDDEN" : response.code;
        response.message = "Acces refuse par le backend SeaTyrants.";
        return;
    }

    if (response.httpStatusCode == 404)
    {
        response.code = response.code.empty() ? "E_BACKEND_NOT_FOUND" : response.code;
        response.message = "Service de connexion SeaTyrants introuvable.";
        return;
    }

    if (response.httpStatusCode == 408)
    {
        response.code = response.code.empty() ? "E_BACKEND_TIMEOUT" : response.code;
        response.message = "Le backend SeaTyrants a mis trop de temps a traiter la connexion.";
        return;
    }

    if (response.httpStatusCode == 422)
    {
        response.code = response.code.empty() ? "E_VALIDATION" : response.code;
        if (!ClientHttp_Auth_SignInRequest_ReadValidationMessage(responseJson, response.message) ||
            response.message.empty())
        {
            response.message = "Informations de connexion invalides.";
        }
        else
        {
            response.message =
                ClientHttp_Auth_SignInRequest_TranslateValidationMessage(responseJson, response.message);
        }
        return;
    }

    if (response.httpStatusCode == 429)
    {
        response.code = response.code.empty() ? "E_RATE_LIMITED" : response.code;
        response.message = "Trop de tentatives de connexion. Reessayez dans un instant.";
        return;
    }

    if (response.httpStatusCode == 502 ||
        response.httpStatusCode == 503 ||
        response.httpStatusCode == 504)
    {
        response.code = response.code.empty() ? "E_BACKEND_UNAVAILABLE" : response.code;
        response.message = "Service de connexion SeaTyrants temporairement indisponible.";
        return;
    }

    if (response.httpStatusCode >= 500)
    {
        response.code = response.code.empty() ? "E_BACKEND_SERVER" : response.code;
        response.message = "Serveur web SeaTyrants temporairement indisponible.";
        return;
    }

    if (response.message.empty())
    {
        response.message = std::string("Echec de la connexion (HTTP ") + std::to_string(response.httpStatusCode) + ").";
    }
}

// Lire un port obligatoire dans un objet JSON et le valider.
static bool ClientHttp_Auth_SignInRequest_ReadRequiredPort(
    const cJSON* jsonObject,
    const char* key,
    uint16_t& outPort)
{
    // Sans objet JSON source, lecture impossible.
    if (jsonObject == nullptr)
    {
        // Echec immediat si l'objet n'existe pas.
        return false;
    }

    // Sans nom de champ, impossible de chercher la valeur.
    if (key == nullptr)
    {
        // Echec immediat si la cle est invalide.
        return false;
    }

    // Recuperer le champ JSON cible par son nom exact.
    const cJSON* portItem = cJSON_GetObjectItemCaseSensitive(jsonObject, key);

    // Le port doit etre present et de type numerique.
    if (!cJSON_IsNumber(portItem))
    {
        // Echec si le champ est absent ou de mauvais type.
        return false;
    }

    // Convertir la valeur numerique JSON en entier C++.
    const int port = portItem->valueint;

    // Refuser les ports invalides ou hors plage uint16 reseau utile.
    if (port <= 0 || port > 65535)
    {
        // Echec si la valeur n'est pas un port reseau valide.
        return false;
    }

    // Copier le port valide vers la sortie typée.
    outPort = static_cast<uint16_t>(port);

    // Signaler que la lecture a reussi.
    return true;
}

// Lire une entree unique de serveur renvoyee par le backend SeaTyrants.
static bool ClientHttp_Auth_SignInRequest_ReadServerEntry(
    const cJSON* serverItem,
    AuthSignInHTTPServerEntry& outServer)
{
    // Chaque entree serveur doit etre un objet JSON.
    if (!cJSON_IsObject(serverItem))
    {
        // Echec si l'element du tableau n'est pas un objet.
        return false;
    }

    // Recuperer le champ booleen indiquant si le serveur est ferme.
    const cJSON* isClosedItem = cJSON_GetObjectItemCaseSensitive(serverItem, "isClosed");

    // Le champ isClosed est accepte en bool JSON ou en entier 0/1 selon le backend.
    if (!cJSON_IsBool(isClosedItem) && !cJSON_IsNumber(isClosedItem))
    {
        // Echec si la structure de l'entree serveur est invalide.
        return false;
    }

    // Lire le nom du serveur dans la structure de sortie.
    if (!ClientHttp_Common_ReadRequiredString(serverItem, "name", outServer.name))
    {
        // Echec si le nom du serveur manque ou n'est pas une string.
        return false;
    }

    // Lire la region du serveur dans la structure de sortie.
    if (!ClientHttp_Common_ReadRequiredString(serverItem, "region", outServer.region))
    {
        // Echec si la region du serveur manque ou n'est pas une string.
        return false;
    }

    // Convertir la valeur booleenne / numerique vers le bool C++ de sortie.
    outServer.isClosed = cJSON_IsBool(isClosedItem)
        ? cJSON_IsTrue(isClosedItem)
        : (isClosedItem->valuedouble != 0.0);

    // Signaler que l'entree serveur est complete et valide.
    return true;
}

// Lire le tableau complet `listServers` renvoye par le backend SeaTyrants.
static bool ClientHttp_Auth_SignInRequest_ReadServerList(
    const cJSON* jsonObject,
    std::vector<AuthSignInHTTPServerEntry>& outServers)
{
    // Sans objet racine JSON, impossible de lire la liste.
    if (jsonObject == nullptr)
    {
        // Echec immediat si le payload racine est invalide.
        return false;
    }

    // Recuperer le tableau JSON contenant les serveurs visibles.
    const cJSON* listServersItem = cJSON_GetObjectItemCaseSensitive(jsonObject, "listServers");

    // Le champ listServers doit exister et etre un tableau JSON.
    if (!cJSON_IsArray(listServersItem))
    {
        // Echec si le backend ne renvoie pas le tableau attendu.
        return false;
    }

    // Nettoyer la sortie avant de reconstruire la liste.
    outServers.clear();

    // Pointeur de parcours utilise par la macro cJSON_ArrayForEach.
    const cJSON* serverItem = nullptr;

    // Iterer sur chaque entree du tableau listServers.
    cJSON_ArrayForEach(serverItem, listServersItem)
    {
        // Initialiser une structure serveur vide pour l'entree courante.
        AuthSignInHTTPServerEntry serverEntry{};

        // Tenter de parser proprement l'entree courante.
        if (!ClientHttp_Auth_SignInRequest_ReadServerEntry(serverItem, serverEntry))
        {
            // Nettoyer la sortie pour ne pas laisser une liste partielle.
            outServers.clear();

            // Echec global si une seule entree est invalide.
            return false;
        }

        // Ajouter l'entree validee dans la liste de sortie.
        outServers.push_back(std::move(serverEntry));
    }

    // Signaler que toute la liste a ete lue avec succes.
    return true;
}

AuthSignInHTTPResponse ClientHttp_Auth_SignInRequest(const AuthSignInHTTPRequest& request)
{
    // Reponse renvoyee au thread HTTP appelant.
    AuthSignInHTTPResponse response{};

    // Valeur defensive par defaut: echec tant que non prouve.
    response.success = false;

    // Recuperer l'etat reseau global pour acceder a la base URL selon l'environnement.
    const NetworkState& networkState = GetNetworkState();

    // Construire l'URL complete de l'endpoint signin expose par SeaTyrants Web Backend.
    const std::string url = std::string(networkState.baseUrlApi) + "/auth/signin";

    // Creer un objet JSON vide pour le payload POST.
    cJSON* jsonBody = cJSON_CreateObject();

    // Si l'allocation JSON echoue, retourner une erreur claire.
    if (jsonBody == nullptr)
    {
        // Remplir un message exploitable cote appelant.
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_REQUEST_BUILD",
            "Le client n'a pas pu preparer la requete de connexion.",
            response);

        // Sortir immediatement sans tenter l'appel HTTP.
        return response;
    }

    // Ajouter l'e-mail du joueur dans le payload JSON.
    cJSON_AddStringToObject(jsonBody, "email", request.email.c_str());

    // Ajouter le mot de passe du joueur dans le payload JSON.
    cJSON_AddStringToObject(jsonBody, "password", request.password.c_str());

    // Serialiser l'objet JSON en texte compact.
    char* jsonBodyText = cJSON_PrintUnformatted(jsonBody);

    // Liberer l'objet JSON source une fois serialise.
    cJSON_Delete(jsonBody);

    // Si la serialisation echoue, retourner une erreur claire.
    if (jsonBodyText == nullptr)
    {
        // Memoriser la cause de l'echec pour diagnostic.
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_REQUEST_BUILD",
            "Le client n'a pas pu serialiser la requete de connexion.",
            response);

        // Sortir immediatement car la requete ne peut pas etre envoyee.
        return response;
    }

    // Copier le texte JSON C dans une string C++ plus simple a manipuler.
    std::string requestBody = jsonBodyText;

    // Liberer le buffer alloue par cJSON.
    cJSON_free(jsonBodyText);

    // Verifier que la taille du body est representable en `long` pour libcurl.
    if (requestBody.size() > static_cast<size_t>((std::numeric_limits<long>::max)()))
    {
        // Signaler explicitement le probleme de taille.
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_REQUEST_TOO_LARGE",
            "Le client a genere une requete de connexion invalide.",
            response);

        // Sortir avant toute initialisation reseau.
        return response;
    }

    // Initialiser un handle libcurl pour cette requete.
    CURL* curl = curl_easy_init();

    // Si libcurl ne peut pas s'initialiser, retourner une erreur claire.
    if (curl == nullptr)
    {
        // Memoriser une erreur exploitable dans la reponse metier.
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_CURL_INIT",
            "Le client n'a pas pu initialiser la connexion HTTP.",
            response);

        // Sortir immediatement car aucun appel HTTP ne peut etre fait.
        return response;
    }

    // Buffer qui accumule le body HTTP de reponse.
    std::string responseBody;

    // Buffer detaille d'erreur rempli par libcurl en cas d'echec.
    char curlErrorBuffer[CURL_ERROR_SIZE] = {0};

    // Liste chainée des headers HTTP envoyes avec la requete.
    curl_slist* headers = nullptr;

    // Ajouter le header indiquant que le body envoye est du JSON.
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (headers == nullptr)
    {
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_HEADERS",
            "Le client n'a pas pu preparer les headers HTTP de connexion.",
            response);

        curl_easy_cleanup(curl);
        return response;
    }

    // Ajouter le header indiquant que le client attend du JSON en retour.
    curl_slist* extendedHeaders = curl_slist_append(headers, "Accept: application/json");
    if (extendedHeaders == nullptr)
    {
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_HEADERS",
            "Le client n'a pas pu preparer les headers HTTP de connexion.",
            response);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    }
    headers = extendedHeaders;

    // Configurer l'URL cible de la requete HTTP.
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Brancher le buffer d'erreur detaille de libcurl.
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuffer);

    // Forcer l'utilisation de la methode HTTP POST.
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Appliquer les headers HTTP prepares plus haut.
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Fournir le corps POST JSON a libcurl.
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());

    // Fournir explicitement la taille du corps POST.
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));

    // Enregistrer le callback qui accumule le body de reponse.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ClientHttp_Common_WriteResponseBodyCallback);

    // Passer la string de destination utilisee par le callback.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    // Desactiver les signaux POSIX pour rester safe en contexte multithread.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // Definir un timeout de connexion raisonnable.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);

    // Definir un timeout global pour toute la requete.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);

    // Buffer textuel pour les erreurs de configuration TLS locales.
    std::string tlsConfigurationError;

    // Configurer la verification TLS si l'URL est en HTTPS.
    if (!ClientHttp_Common_ConfigureTlsForHttps(curl, url, tlsConfigurationError))
    {
        // Confirmer explicitement l'echec cote metier.
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_TLS_CONFIG",
            "La configuration TLS du backend SeaTyrants est invalide.",
            response);

        // Liberer la liste des headers allouee.
        curl_slist_free_all(headers);

        // Liberer le handle curl avant de sortir.
        curl_easy_cleanup(curl);

        // Logger l'erreur pour diagnostic technique.
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] %s technical=%s",
            response.message.c_str(),
            tlsConfigurationError.c_str());

        // Retourner immediatement la reponse d'erreur.
        return response;
    }

    // Logger l'endpoint appele pour faciliter les traces reseau.
    RC2D_log(
        RC2D_LOG_INFO,
        "[CLIENT] [HTTP] [AUTH_SIGNIN] POST %s",
        url.c_str());

    // Executer la requete HTTP synchrone.
    const CURLcode curlCode = curl_easy_perform(curl);

    // Variable de stockage du code HTTP brut retourne par le serveur.
    long httpStatusCode = 0;

    // Recuperer le status HTTP meme si la requete a echoue cote transport.
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatusCode);

    // Liberer la liste des headers HTTP apres l'appel.
    curl_slist_free_all(headers);

    // Liberer le handle curl une fois l'operation terminee.
    curl_easy_cleanup(curl);

    // Gerer les echecs de transport libcurl (DNS, TLS, socket, timeout...).
    if (curlCode != CURLE_OK)
    {
        // Conserver l'etat d'echec dans la reponse metier.
        response.success = false;
        response.httpStatusCode = httpStatusCode;

        // Preferer le buffer detaille si libcurl l'a rempli.
        const char* detailedError = (curlErrorBuffer[0] != '\0')
            ? curlErrorBuffer
            : curl_easy_strerror(curlCode);

        // Construire un code/message clair pour le joueur.
        ClientHttp_Auth_SignInRequest_MapTransportFailure(curlCode, response);

        // Logger l'erreur technique detaillee.
        RC2D_log(
            RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] CURL error=%d publicCode=%s technical=%s",
            static_cast<int>(curlCode),
            response.code.c_str(),
            detailedError);

        // Retourner immediatement la reponse d'echec transport.
        return response;
    }

    // Memoriser le status HTTP brut dans la reponse metier.
    response.httpStatusCode = httpStatusCode;

    // Un transfert curl reussi sans veritable statut HTTP reste inexploitable.
    if (httpStatusCode <= 0)
    {
        ClientHttp_Auth_SignInRequest_SetClientFailure(
            "E_BACKEND_INVALID_RESPONSE",
            "Le backend SeaTyrants a renvoye une reponse HTTP invalide.",
            response);
        return response;
    }

    // Initialiser le pointeur JSON de reponse a nul par defaut.
    cJSON* responseJson = nullptr;

    // Parser le body JSON uniquement si le serveur a renvoye quelque chose.
    if (!responseBody.empty())
    {
        // Tenter de parser le body HTTP brut en arbre cJSON.
        responseJson = cJSON_Parse(responseBody.c_str());
    }

    // Gerer le cas de succes attendu par le backend SeaTyrants.
    if (httpStatusCode == 200)
    {
        // Sans JSON valide, le succes HTTP ne suffit pas pour accepter la reponse.
        if (responseJson == nullptr)
        {
            // Conserver l'echec metier si le payload est inutilisable.
            ClientHttp_Auth_SignInRequest_SetClientFailure(
                "E_BACKEND_PAYLOAD",
                "Le backend SeaTyrants a renvoye une reponse invalide.",
                response);
            response.httpStatusCode = httpStatusCode;
        }
        else
        {
            // Lire le bearer token opaque CrzGames renvoye par SeaTyrants.
            const bool tokenOk = ClientHttp_Common_ReadRequiredString(
                responseJson,
                "crzgames_token_bearer",
                response.crzgamesTokenBearer);

            // Lire le DNS Quilkin renvoye pour la connexion reseau du client.
            const bool quilkinDnsOk =
                ClientHttp_Common_ReadRequiredString(responseJson, "quilkinDns", response.quilkinDns);

            // Lire le port Quilkin renvoye pour la connexion reseau du client.
            const bool quilkinPortOk =
                ClientHttp_Auth_SignInRequest_ReadRequiredPort(responseJson, "quilkinPort", response.quilkinPort);

            // Lire la liste des serveurs visibles par ce joueur.
            const bool listServersOk =
                ClientHttp_Auth_SignInRequest_ReadServerList(responseJson, response.listServers);

            // Le succes final n'est valide que si tout le payload obligatoire est present.
            response.success = tokenOk && quilkinDnsOk && quilkinPortOk && listServersOk;

            // Fournir un message metier coherent selon le resultat du parsing.
            if (response.success)
            {
                // Message simple en cas de succes complet.
                response.message = "Connexion web reussie.";
            }
            else
            {
                // Message explicite si le contrat JSON n'est pas respecte.
                response.code = "E_BACKEND_PAYLOAD";
                response.message = "La reponse de connexion du backend est incomplete.";

                RC2D_log(
                    RC2D_LOG_ERROR,
                    "[CLIENT] [HTTP] [AUTH_SIGNIN] Payload flags token=%u quilkinDns=%u quilkinPort=%u listServers=%u",
                    tokenOk ? 1u : 0u,
                    quilkinDnsOk ? 1u : 0u,
                    quilkinPortOk ? 1u : 0u,
                    listServersOk ? 1u : 0u);
            }
        }

        // Liberer l'arbre JSON s'il a ete cree.
        if (responseJson != nullptr)
        {
            // Nettoyer l'allocation cJSON de la reponse parsee.
            cJSON_Delete(responseJson);
        }

        // Logger le resultat final du signin HTTP.
        RC2D_log(
            response.success ? RC2D_LOG_INFO : RC2D_LOG_ERROR,
            "[CLIENT] [HTTP] [AUTH_SIGNIN] HTTP status=%ld success=%u message=%s",
            response.httpStatusCode,
            response.success ? 1u : 0u,
            response.message.c_str());

        // Retourner immediatement la reponse de succes ou de payload invalide.
        return response;
    }

    // Marquer explicitement les autres statuts HTTP comme des echecs metier.
    response.success = false;

    // Si un JSON d'erreur existe, tenter de lire son code et son message.
    if (responseJson != nullptr)
    {
        // Lire le code metier renvoye par le backend si present.
        ClientHttp_Common_ReadRequiredString(responseJson, "code", response.code);

        // Lire le message metier renvoye par le backend si present.
        ClientHttp_Common_ReadRequiredString(responseJson, "message", response.message);
    }

    // Convertir les reponses backend brutes en messages propres cote client.
    ClientHttp_Auth_SignInRequest_NormalizeHttpFailureMessage(response, responseJson);

    // Liberer l'arbre JSON de reponse d'erreur apres lecture.
    if (responseJson != nullptr)
    {
        cJSON_Delete(responseJson);
    }

    // Logger le resultat final de la reponse d'erreur.
    RC2D_log(
        RC2D_LOG_WARN,
        "[CLIENT] [HTTP] [AUTH_SIGNIN] HTTP status=%ld success=%u code=%s message=%s",
        response.httpStatusCode,
        response.success ? 1u : 0u,
        response.code.empty() ? "<none>" : response.code.c_str(),
        response.message.c_str());

    // Retourner la reponse finale au thread HTTP appelant.
    return response;
}
