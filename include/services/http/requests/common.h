#pragma once

#include <cstddef> // size_t
#include <string>  // std::string

#include <curl/curl.h> // CURL

struct cJSON;

// Callback libcurl:
// - appelee par libcurl pour chaque chunk recu
// - copie le chunk dans la string de sortie
size_t ClientHttp_Common_WriteResponseBodyCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userData);

// Lit un champ string obligatoire dans un objet JSON.
bool ClientHttp_Common_ReadRequiredString(
    const cJSON* jsonObject,
    const char* key,
    std::string& outValue);

// Configure les options TLS cURL pour les endpoints HTTPS.
bool ClientHttp_Common_ConfigureTlsForHttps(
    CURL* curl,
    const std::string& url,
    std::string& outError);
