#include "core/client-language-state.h"

bool ClientLanguageState::hasDetectedLocale() const
{
    // Exposer un test explicite garde l'appelant lisible et evite qu'il doive
    // deduire lui-meme l'etat a partir d'une chaine vide.
    return this->detectedLocaleAvailable;
}

const std::string& ClientLanguageState::getDetectedLanguageCode() const
{
    // Renvoyer une reference constante evite les copies inutiles sur un acces
    // qui peut etre frequemment consulte par les scenes UI.
    return this->detectedLanguageCode;
}

const std::string& ClientLanguageState::getDetectedCountryCode() const
{
    // Le code pays reste consulte tel quel par l'UI pour formater la locale.
    return this->detectedCountryCode;
}

const std::string& ClientLanguageState::getDetectedFlagName() const
{
    // Ce drapeau represente la meilleure traduction visuelle de la detection OS.
    return this->detectedFlagName;
}

const std::string& ClientLanguageState::getCurrentFlagName() const
{
    // Ce drapeau correspond au choix effectivement visible dans l'UI du menu.
    return this->currentFlagName;
}

void ClientLanguageState::setDetectedLocale(
    const std::string& languageCode,
    const std::string& countryCode,
    const std::string& detectedFlagNameValue)
{
    // Conserver l'ancien drapeau detecte permet de savoir si le drapeau courant
    // suivait encore la detection systeme ou s'il avait ete remplace manuellement.
    const std::string previousDetectedFlagName = this->detectedFlagName;

    // Memoriser la detection brute pour les futures lectures globales du client.
    this->detectedLanguageCode = languageCode;
    this->detectedCountryCode = countryCode;
    this->detectedFlagName = detectedFlagNameValue;
    this->detectedLocaleAvailable = !languageCode.empty();

    // Synchroniser le drapeau courant uniquement si aucun choix specifique n'a
    // encore ete fait ou si le drapeau courant suivait deja l'ancien detecte.
    if (this->currentFlagName.empty() || this->currentFlagName == previousDetectedFlagName)
    {
        this->currentFlagName = detectedFlagNameValue;
    }
}

void ClientLanguageState::setCurrentFlagName(const std::string& currentFlagNameValue)
{
    // Enregistrer directement le choix UI courant rend l'etat reutilisable
    // partout sans devoir propager un parametre supplementaire.
    this->currentFlagName = currentFlagNameValue;
}
