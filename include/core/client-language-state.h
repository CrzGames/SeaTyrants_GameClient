#pragma once

#include <string>

/**
 * @brief Etat runtime de la langue client detectee et du drapeau actif.
 *
 * Cette classe isole la gestion de la langue systeme detectee et du drapeau
 * actuellement actif dans l'UI. Elle evite de disperser des champs + setters
 * libres dans `context.h`, tout en gardant une API simple pour le reste du client.
 */
class ClientLanguageState
{
public:
    /**
     * @brief Retourne true si une locale systeme valide a deja ete detectee.
     */
    bool hasDetectedLocale() const;

    /**
     * @brief Retourne le code langue ISO-639 detecte.
     */
    const std::string& getDetectedLanguageCode() const;

    /**
     * @brief Retourne le code pays ISO-3166 detecte.
     */
    const std::string& getDetectedCountryCode() const;

    /**
     * @brief Retourne le nom de drapeau derive de la detection systeme.
     */
    const std::string& getDetectedFlagName() const;

    /**
     * @brief Retourne le nom du drapeau actuellement actif dans l'UI.
     */
    const std::string& getCurrentFlagName() const;

    /**
     * @brief Memorise la locale systeme detectee et son drapeau derive.
     *
     * Tant que le drapeau courant n'a pas ete explicitement remplace par autre
     * chose, il suit automatiquement le drapeau detecte.
     *
     * @param languageCode Code langue ISO-639 detecte.
     * @param countryCode Code pays ISO-3166 detecte.
     * @param detectedFlagNameValue Nom de drapeau derive de cette detection.
     */
    void setDetectedLocale(
        const std::string& languageCode,
        const std::string& countryCode,
        const std::string& detectedFlagNameValue);

    /**
     * @brief Met a jour le drapeau actuellement actif dans l'UI.
     *
     * @param currentFlagNameValue Nom du drapeau choisi dans `assets/images/languages/`.
     */
    void setCurrentFlagName(const std::string& currentFlagNameValue);

private:
    std::string detectedLanguageCode{}; /**< Code langue ISO-639 detecte. */
    std::string detectedCountryCode{};  /**< Code pays ISO-3166 detecte. */
    std::string detectedFlagName{};     /**< Drapeau derive de la locale systeme. */
    std::string currentFlagName{};      /**< Drapeau actuellement actif dans l'UI. */
    bool detectedLocaleAvailable = false; /**< True si une locale a ete detectee. */
};
