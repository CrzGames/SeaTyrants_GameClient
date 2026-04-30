#pragma once

#include <string> // std::string

// ============================================================================
// Request HTTP du client vers le backend d'authentification pour se connecter
// a un compte SeaTyrants existant.
// ============================================================================
struct AuthSignInHTTPRequest
{
    // Adresse e-mail du compte CrzGames utilise pour se connecter au jeu.
    std::string email;

    // Mot de passe du compte CrzGames.
    std::string password;
};
