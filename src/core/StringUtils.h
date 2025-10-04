// ============================================================================
// Fichier: src/core/StringUtils.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Utilitaires pour la manipulation de chaînes de caractères.
//   Fonctions helper pour parsing, formatting, conversion.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace midiMind {
namespace StringUtils {

/**
 * @brief Trim whitespace à gauche
 */
inline std::string ltrim(const std::string& str) {
    std::string result = str;
    result.erase(result.begin(), 
                std::find_if(result.begin(), result.end(), 
                [](unsigned char ch) { return !std::isspace(ch); }));
    return result;
}

/**
 * @brief Trim whitespace à droite
 */
inline std::string rtrim(const std::string& str) {
    std::string result = str;
    result.erase(std::find_if(result.rbegin(), result.rend(),
                [](unsigned char ch) { return !std::isspace(ch); }).base(), 
                result.end());
    return result;
}

/**
 * @brief Trim whitespace des deux côtés
 */
inline std::string trim(const std::string& str) {
    return ltrim(rtrim(str));
}

/**
 * @brief Convertit en minuscules
 */
inline std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief Convertit en majuscules
 */
inline std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::toupper(c); });
    return result;
}

/**
 * @brief Sépare une string selon un délimiteur
 * 
 * @param str String à séparer
 * @param delimiter Délimiteur
 * @return std::vector<std::string> Parties
 * 
 * @example
 * ```cpp
 * auto parts = StringUtils::split("a,b,c", ',');
 * // {"a", "b", "c"}
 * ```
 */
inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

/**
 * @brief Joint des strings avec un séparateur
 * 
 * @param strings Strings à joindre
 * @param separator Séparateur
 * @return std::string String jointe
 * 
 * @example
 * ```cpp
 * std::vector<std::string> parts = {"a", "b", "c"};
 * auto joined = StringUtils::join(parts, ", ");
 * // "a, b, c"
 * ```
 */
inline std::string join(const std::vector<std::string>& strings, 
                       const std::string& separator) {
    if (strings.empty()) return "";
    
    std::ostringstream oss;
    oss << strings[0];
    
    for (size_t i = 1; i < strings.size(); ++i) {
        oss << separator << strings[i];
    }
    
    return oss.str();
}

/**
 * @brief Vérifie si une string commence par un préfixe
 */
inline bool startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

/**
 * @brief Vérifie si une string se termine par un suffixe
 */
inline bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

/**
 * @brief Remplace toutes les occurrences d'une substring
 * 
 * @param str String source
 * @param from Substring à remplacer
 * @param to Substring de remplacement
 * @return std::string String modifiée
 */
inline std::string replaceAll(std::string str, 
                              const std::string& from, 
                              const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

/**
 * @brief Convertit un nombre en string hexadécimal
 * 
 * @param value Valeur
 * @param width Largeur (avec padding zeros)
 * @return std::string Hex string (ex: "0x1A")
 */
template<typename T>
inline std::string toHex(T value, int width = 0) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex;
    
    if (width > 0) {
        oss << std::setfill('0') << std::setw(width);
    }
    
    oss << static_cast<uint64_t>(value);
    return oss.str();
}

/**
 * @brief Convertit une string hexadécimal en nombre
 * 
 * @param hexStr String hex (avec ou sans "0x")
 * @return T Valeur numérique
 */
template<typename T>
inline T fromHex(const std::string& hexStr) {
    std::string str = hexStr;
    
    // Retirer le préfixe "0x" si présent
    if (startsWith(str, "0x") || startsWith(str, "0X")) {
        str = str.substr(2);
    }
    
    std::istringstream iss(str);
    T value;
    iss >> std::hex >> value;
    
    return value;
}

/**
 * @brief Formate une taille en bytes de manière lisible
 * 
 * @param bytes Nombre de bytes
 * @return std::string Taille formatée (ex: "1.5 MB")
 */
inline std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

/**
 * @brief Génère un UUID simple
 * 
 * @return std::string UUID (format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
 * 
 * @note Version simplifiée, pas cryptographiquement sécurisée
 */
inline std::string generateUuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::ostringstream oss;
    oss << std::hex;
    
    for (int i = 0; i < 8; i++) oss << dis(gen);
    oss << "-";
    for (int i = 0; i < 4; i++) oss << dis(gen);
    oss << "-4"; // Version 4
    for (int i = 0; i < 3; i++) oss << dis(gen);
    oss << "-";
    oss << dis2(gen); // Variant
    for (int i = 0; i < 3; i++) oss << dis(gen);
    oss << "-";
    for (int i = 0; i < 12; i++) oss << dis(gen);
    
    return oss.str();
}

/**
 * @brief Pad une string à gauche
 * 
 * @param str String source
 * @param width Largeur cible
 * @param fillChar Caractère de remplissage
 * @return std::string String paddée
 */
inline std::string padLeft(const std::string& str, size_t width, char fillChar = ' ') {
    if (str.length() >= width) return str;
    return std::string(width - str.length(), fillChar) + str;
}

/**
 * @brief Pad une string à droite
 * 
 * @param str String source
 * @param width Largeur cible
 * @param fillChar Caractère de remplissage
 * @return std::string String paddée
 */
inline std::string padRight(const std::string& str, size_t width, char fillChar = ' ') {
    if (str.length() >= width) return str;
    return str + std::string(width - str.length(), fillChar);
}

/**
 * @brief Convertit un bytes array en string hexadécimal
 * 
 * @param data Données
 * @param size Taille
 * @param separator Séparateur entre bytes (défaut: " ")
 * @return std::string Hex string
 * 
 * @example
 * ```cpp
 * uint8_t data[] = {0x90, 0x3C, 0x64};
 * auto hex = StringUtils::bytesToHex(data, 3);
 * // "90 3C 64"
 * ```
 */
inline std::string bytesToHex(const uint8_t* data, size_t size, 
                              const std::string& separator = " ") {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    
    for (size_t i = 0; i < size; ++i) {
        if (i > 0) oss << separator;
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return oss.str();
}

} // namespace StringUtils
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER StringUtils.h
// ============================================================================