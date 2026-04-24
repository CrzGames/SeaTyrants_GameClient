#pragma once

#include <string>

#include <RC2D/RC2D.h>

/**
 * @brief Resolveur qui traduit une locale systeme RC2D/SDL en etat de langue client.
 *
 * Cette classe centralise :
 * - la normalisation des codes langue/pays ;
 * - le mapping locale -> drapeau projet ;
 * - l'ecriture du resultat dans l'etat global de langue du client.
 */
class SystemLocaleLanguageResolver
{
public:
    /**
     * @brief Renvoie une copie ASCII en minuscules pour les comparaisons de codes.
     *
     * @param value Chaine source potentiellement nulle.
     * @return Copie normalisee en minuscules ASCII.
     */
    static std::string ToLowerAsciiCopy(const char* value);

    /**
     * @brief Associe un code pays ISO-3166 a un drapeau disponible dans le projet.
     *
     * @param countryCode Code pays deja normalise en minuscules.
     * @return Nom de drapeau sans extension, ou chaine vide si aucun mapping n'existe.
     */
    static std::string ResolveFlagFromCountryCode(const std::string& countryCode);

    /**
     * @brief Associe un code langue ISO-639 a un drapeau de repli du projet.
     *
     * @param languageCode Code langue deja normalise en minuscules.
     * @return Nom de drapeau sans extension, ou chaine vide si aucun mapping n'existe.
     */
    static std::string ResolveFlagFromLanguageCode(const std::string& languageCode);

    /**
     * @brief Choisit le drapeau le plus pertinent a partir d'une locale systeme.
     *
     * Le pays est prioritaire si un drapeau exact existe. Sinon on retombe sur
     * un drapeau representatif de la langue detectee.
     *
     * @param languageCode Code langue brut issu de RC2D_local / SDL.
     * @param countryCode Code pays brut issu de RC2D_local / SDL.
     * @return Nom de drapeau sans extension.
     */
    static std::string ResolveFlagNameForLocale(const char* languageCode, const char* countryCode);

    /**
     * @brief Memorise la premiere locale systeme exploitable dans l'etat global client.
     *
     * @param locales Tableau RC2D_Locale null-termine retourne par RC2D_local.
     */
    static void StorePreferredLocaleInContext(const RC2D_Locale* locales);
};
