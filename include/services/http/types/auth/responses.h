#pragma once

#include <cstdint> // uint16_t
#include <string>  // std::string

// ============================================================================
// Response HTTP du backend d'authentification vers le client pour la requete de creation de compte.
// HTTP STATUS: 201 Created = succes de creation
// ============================================================================
struct AuthSignUpHTTPResponse
{
    // True uniquement quand le backend renvoie HTTP 201.
    bool success = false;

    // Code HTTP brut de la reponse backend (ex: 201, 500).
    long httpStatusCode = 0;

    // Code d'erreur backend quand present (ex: "E_INTERNAL_SERVER_ERROR").
    std::string code;

    // Message backend (succes ou erreur).
    std::string message;
};

// ============================================================================
// Response HTTP du backend d'authentification vers le client pour la requete de connexion.
// HTTP STATUS: 200 OK = succes de connexion
// ============================================================================
struct AuthSignInHTTPResponse
{
    // True uniquement quand le backend renvoie HTTP 200 avec payload token valide.
    bool success = false;

    // Code HTTP brut de la reponse backend (ex: 200, 500).
    long httpStatusCode = 0;

    // Code d'erreur backend quand present (ex: "E_INTERNAL_SERVER_ERROR").
    std::string code;

    // Message backend (souvent present en cas d'erreur).
    std::string message;

    // Champs retournes en succes (HTTP 200):
    // {
    //   "token": { "type": "...", "value": "...", "expiresAt": "..." },
    //   "quilkin_dns": "...",
    //   "quilkin_port": 7777
    // }
    std::string tokenType;
    std::string tokenValue;
    std::string tokenExpiresAt;
    std::string quilkinDns;
    uint16_t quilkinPort = 0;
};
