#pragma once

#include <string> // std::string

// ============================================================================
// Request HTTP du client vers le backend d'authentification pour créer un nouveau compte.
// ============================================================================
struct AuthSignUpHTTPRequest
{
    std::string username;
    std::string email;
    std::string password;
};

// ============================================================================
// Request HTTP du client vers le backend d'authentification pour se connecter à un compte existant.
// ============================================================================
struct AuthSignInHTTPRequest
{
    std::string email;
    std::string password;
};