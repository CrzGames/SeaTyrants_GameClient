#pragma once

#include <cstdint> // uint16_t
#include <string>  // std::string
#include <vector>  // std::vector

// ============================================================================
// Entree minimale d'un serveur renvoye par le backend SeaTyrants apres signin.
// ============================================================================
struct AuthSignInHTTPServerEntry
{
    // Indique si le serveur est actuellement ferme.
    bool isClosed = false;

    // Nom lisible du serveur.
    std::string name;

    // Region du serveur affichee au joueur.
    std::string region;
};

// ============================================================================
// Response HTTP du backend d'authentification vers le client pour la requete
// de connexion.
// HTTP STATUS: 200 OK = succes de connexion
// ============================================================================
struct AuthSignInHTTPResponse
{
    // True uniquement quand le backend renvoie HTTP 200 avec payload valide.
    bool success = false;

    // Code HTTP brut de la reponse backend (ex: 200, 401, 500).
    long httpStatusCode = 0;

    // Code d'erreur backend quand present (ex: "E_UNAUTHORIZED").
    std::string code;

    // Message backend (souvent present en cas d'erreur).
    std::string message;

    // Bearer token opaque CrzGames a reutiliser pour les appels authentifies.
    std::string crzgamesTokenBearer;

    // Endpoint Quilkin renvoye par le backend SeaTyrants.
    std::string quilkinDns;
    uint16_t quilkinPort = 0;

    // Liste minimale des serveurs affichables au joueur.
    std::vector<AuthSignInHTTPServerEntry> listServers;
};
