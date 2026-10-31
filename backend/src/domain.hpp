#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace signature {

inline constexpr std::array<const char*, 24> skills = {
    "Attack", "Defence", "Strength", "Hitpoints", "Ranged", "Prayer",
    "Magic", "Cooking", "Woodcutting", "Fletching", "Fishing", "Firemaking",
    "Crafting", "Smithing", "Mining", "Herblore", "Agility", "Thieving",
    "Slayer", "Farming", "Runecraft", "Hunter", "Construction", "Sailing"
};

inline bool isAllowedNameCharacter(unsigned char character) {
    const bool isLowercaseLetter = character >= 'a' && character <= 'z';
    const bool isUppercaseLetter = character >= 'A' && character <= 'Z';
    const bool isDigit = character >= '0' && character <= '9';
    const bool isSeparator = character == ' ' || character == '_' || character == '-';

    return isLowercaseLetter || isUppercaseLetter || isDigit || isSeparator;
}

inline std::string normalizeName(std::string name) {
    const auto firstCharacter = name.find_first_not_of(' ');

    if (firstCharacter == std::string::npos) {
        throw std::invalid_argument("Player name must contain 1-12 characters.");
    }

    const auto lastCharacter = name.find_last_not_of(' ');
    name = name.substr(firstCharacter, lastCharacter - firstCharacter + 1);

    if (name.size() > 12) {
        throw std::invalid_argument("Player name must contain 1-12 characters.");
    }

    for (unsigned char character : name) {
        if (!isAllowedNameCharacter(character)) {
            throw std::invalid_argument("Invalid player name.");
        }
    }

    return name;
}

// Case and underscore differences should not create separate player cache entries.
inline std::string keyName(std::string name) {
    std::string normalizedName = normalizeName(std::move(name));

    for (char& character : normalizedName) {
        if (character == '_') {
            character = ' ';
        }

        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))
        );
    }

    return normalizedName;
}

inline int combat(
    int attackLevel,
    int strengthLevel,
    int defenceLevel,
    int hitpointsLevel,
    int rangedLevel,
    int prayerLevel,
    int magicLevel
) {
    const std::array<int, 7> combatSkillLevels = {
        attackLevel,
        strengthLevel,
        defenceLevel,
        hitpointsLevel,
        rangedLevel,
        prayerLevel,
        magicLevel
    };

    for (int level : combatSkillLevels) {
        if (level < 1 || level > 99) {
            throw std::invalid_argument("Combat skills are unavailable or invalid.");
        }
    }

    const int meleeStrength = attackLevel + strengthLevel;
    const int rangedStrength = (rangedLevel * 3) / 2;
    const int magicStrength = (magicLevel * 3) / 2;
    const int strongestStyle = std::max({
        meleeStrength,
        rangedStrength,
        magicStrength
    });

    const int baseContribution = 10 * (
        defenceLevel + hitpointsLevel + prayerLevel / 2
    );
    const int styleContribution = 13 * strongestStyle;

    // Integer arithmetic preserves OSRS's floor operations at level boundaries.
    return (baseContribution + styleContribution) / 40;
}

inline bool validId(std::string_view identifier) {
    if (identifier.size() != 32) {
        return false;
    }

    for (char character : identifier) {
        const bool isDigit = character >= '0' && character <= '9';
        const bool isHexLetter = character >= 'a' && character <= 'f';

        if (!isDigit && !isHexLetter) {
            return false;
        }
    }

    return true;
}

} // namespace signature
