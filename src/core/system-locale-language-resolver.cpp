#include "core/system-locale-language-resolver.h"

#include "core/context.h"

#include <algorithm>
#include <cctype>

std::string SystemLocaleLanguageResolver::ToLowerAsciiCopy(const char* value)
{
    // Convertir une chaine potentiellement nulle en std::string simplifie toute
    // la suite du pipeline sans multiplier les gardes null cote appelant.
    std::string lowered = (value != nullptr) ? std::string(value) : std::string{};

    // Normaliser caractere par caractere en minuscules ASCII stabilise les
    // comparaisons de codes langue/pays quelle que soit la casse fournie par SDL.
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Renvoyer la copie normalisee prete a etre mappee.
    return lowered;
}

std::string SystemLocaleLanguageResolver::ResolveFlagFromCountryCode(const std::string& countryCode)
{
    // Mapper directement les pays pour obtenir le drapeau le plus precis
    // possible quand le systeme fournit deja un code pays exploitable.
    if (countryCode == "al") return "albania";
    if (countryCode == "at") return "austria";
    if (countryCode == "be") return "belgium";
    if (countryCode == "hr") return "croatia";
    if (countryCode == "cz") return "czech_republic";
    if (countryCode == "dk") return "denmark";
    if (countryCode == "gb" || countryCode == "uk") return "england";
    if (countryCode == "fr") return "france";
    if (countryCode == "ge") return "georgia";
    if (countryCode == "de") return "germany";
    if (countryCode == "hu") return "hungary";
    if (countryCode == "it") return "italy";
    if (countryCode == "nl") return "netherlands";
    if (countryCode == "pl") return "poland";
    if (countryCode == "pt") return "portugal";
    if (countryCode == "ro") return "romania";
    if (countryCode == "rs") return "serbia";
    if (countryCode == "sk") return "slovakia";
    if (countryCode == "si") return "slovenia";
    if (countryCode == "es") return "spain";
    if (countryCode == "ch") return "switzerland";
    if (countryCode == "tr") return "turkey";
    if (countryCode == "ua") return "ukraine";
    return {};
}

std::string SystemLocaleLanguageResolver::ResolveFlagFromLanguageCode(const std::string& languageCode)
{
    // Conserver un mapping par langue sert de repli quand le pays manque
    // ou quand il n'existe pas encore de drapeau dedie dans le projet.
    if (languageCode == "sq") return "albania";
    if (languageCode == "de") return "germany";
    if (languageCode == "nl") return "netherlands";
    if (languageCode == "pl") return "poland";
    if (languageCode == "fr") return "france";
    if (languageCode == "en") return "england";
    if (languageCode == "ka") return "georgia";
    if (languageCode == "hr") return "croatia";
    if (languageCode == "cs") return "czech_republic";
    if (languageCode == "da") return "denmark";
    if (languageCode == "hu") return "hungary";
    if (languageCode == "it") return "italy";
    if (languageCode == "pt") return "portugal";
    if (languageCode == "ro") return "romania";
    if (languageCode == "sr") return "serbia";
    if (languageCode == "sk") return "slovakia";
    if (languageCode == "sl") return "slovenia";
    if (languageCode == "es") return "spain";
    if (languageCode == "tr") return "turkey";
    if (languageCode == "uk") return "ukraine";
    return {};
}

std::string SystemLocaleLanguageResolver::ResolveFlagNameForLocale(const char* languageCode, const char* countryCode)
{
    // Normaliser les deux composantes de locale avant d'attaquer les mappings.
    const std::string normalizedCountryCode = ToLowerAsciiCopy(countryCode);
    const std::string normalizedLanguageCode = ToLowerAsciiCopy(languageCode);

    // Prioriser le pays quand un drapeau explicite existe, car c'est la
    // traduction visuelle la plus precise du systeme utilisateur.
    const std::string countryFlagName = ResolveFlagFromCountryCode(normalizedCountryCode);
    if (!countryFlagName.empty())
    {
        return countryFlagName;
    }

    // Sinon, retomber sur un drapeau representatif de la langue detectee.
    const std::string languageFlagName = ResolveFlagFromLanguageCode(normalizedLanguageCode);
    if (!languageFlagName.empty())
    {
        return languageFlagName;
    }

    // Garder un fallback stable et previsible quand aucune correspondance n'existe.
    return "england";
}

void SystemLocaleLanguageResolver::StorePreferredLocaleInContext(const RC2D_Locale* locales)
{
    const char* languageCode = nullptr;
    const char* countryCode = nullptr;
    int chosenLocaleIndex = -1;

    // RC2D_local renvoie maintenant une liste null-terminee ; on prend la
    // premiere locale vraiment exploitable, avec mapping explicite prioritaire.
    if (locales != nullptr)
    {
        int fallbackLocaleIndex = -1;

        for (int index = 0; locales[index].language != nullptr; ++index)
        {
            const std::string normalizedLanguageCode = ToLowerAsciiCopy(locales[index].language);
            const std::string normalizedCountryCode = ToLowerAsciiCopy(locales[index].country);

            if (normalizedLanguageCode.empty() && normalizedCountryCode.empty())
            {
                continue;
            }

            if (fallbackLocaleIndex < 0)
            {
                fallbackLocaleIndex = index;
            }

            const bool hasCountryMatch = !ResolveFlagFromCountryCode(normalizedCountryCode).empty();
            const bool hasLanguageMatch = !ResolveFlagFromLanguageCode(normalizedLanguageCode).empty();
            if (hasCountryMatch || hasLanguageMatch)
            {
                chosenLocaleIndex = index;
                break;
            }
        }

        if (chosenLocaleIndex < 0)
        {
            chosenLocaleIndex = fallbackLocaleIndex;
        }

        if (chosenLocaleIndex >= 0)
        {
            languageCode = locales[chosenLocaleIndex].language;
            countryCode = locales[chosenLocaleIndex].country;
        }
    }

    // Stabiliser les codes avant de les injecter dans l'etat global.
    const std::string normalizedLanguageCode = ToLowerAsciiCopy(languageCode);
    const std::string normalizedCountryCode = ToLowerAsciiCopy(countryCode);

    // Deriver le drapeau qui servira de choix initial dans le menu.
    const std::string detectedFlagName = ResolveFlagNameForLocale(languageCode, countryCode);

    // Ecrire le resultat dans l'etat global partage par les scenes.
    GetClientLanguageState().setDetectedLocale(
        normalizedLanguageCode.empty() ? std::string("en") : normalizedLanguageCode,
        normalizedCountryCode,
        detectedFlagName);

    // Logger explicitement la resolution aide a verifier rapidement le mapping
    // en dev et lors des tests multi-langues.
    RC2D_log(
        RC2D_LOG_INFO,
        "Client locale detected: index=%d language=%s country=%s flag=%s",
        chosenLocaleIndex,
        normalizedLanguageCode.empty() ? "en" : normalizedLanguageCode.c_str(),
        normalizedCountryCode.empty() ? "-" : normalizedCountryCode.c_str(),
        detectedFlagName.c_str());
}
