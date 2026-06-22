#pragma once
// World-generation parameters and their option lists.
//
// WorldParams captures the player's choices for a generated world; the option
// arrays back the New Game form's dropdowns and bound the values a structured
// world-generation call may receive. All lists are plain data so the UI and the
// engine can share them without duplication.

#include <array>
#include <string>
#include <vector>

namespace oce {

struct WorldParams {
    std::string biome;
    std::string culture;
    std::string population;
    std::string technology;
    std::string political;
    std::string magic;
    std::string species;
    std::string threat;
    std::vector<std::string> custom_fields; // up to five free-form descriptors
};

inline constexpr std::array<const char*, 16> BIOME_OPTIONS = {{
    "Tropical Rainforest",
    "Temperate Deciduous Forest",
    "Coniferous Forest (Taiga)",
    "Grassland (Savanna)",
    "Grassland (Prairie)",
    "Desert",
    "Tundra",
    "Mediterranean Shrubland",
    "Wetland (Marsh)",
    "Wetland (Swamp)",
    "Coastal",
    "Mountain Range",
    "Urban Metropolis",
    "Urban Sprawl",
    "Megacity",
    "Post-Industrial Wasteland",
}};

inline constexpr std::array<const char*, 14> CULTURE_OPTIONS = {{
    "Nomadic Tribes",
    "Agricultural Villages",
    "Maritime Traders",
    "Mountain Clans",
    "Desert Caravans",
    "Forest Dwellers",
    "City-States",
    "Imperial Society",
    "Merchant Guilds",
    "Monastic Orders",
    "Corporate Enclaves",
    "Tech Communes",
    "Megacorp Districts",
    "Underground Networks",
}};

inline constexpr std::array<const char*, 5> POPULATION_OPTIONS = {{
    "Uninhabited",
    "Sparse (Remote settlements)",
    "Moderate (Small towns)",
    "Dense (Cities and towns)",
    "Very Dense (Metropolitan)",
}};

inline constexpr std::array<const char*, 9> TECHNOLOGY_OPTIONS = {{
    "Stone Age (Primitive tools)",
    "Bronze Age (Early metalworking)",
    "Iron Age (Advanced metalworking)",
    "Medieval (Feudal era)",
    "Renaissance (Early modern)",
    "Industrial (Steam power)",
    "Modern (Present day)",
    "Cyberpunk (Near future)",
    "Mixed (Varied by region)",
}};

inline constexpr std::array<const char*, 8> POLITICAL_OPTIONS = {{
    "Tribal (Chieftain leadership)",
    "Monarchy (Hereditary rule)",
    "Republic (Elected councils)",
    "Empire (Centralized power)",
    "Theocracy (Religious rule)",
    "Anarchy (No central authority)",
    "Confederation (Allied states)",
    "Oligarchy (Rule by few)",
}};

inline constexpr std::array<const char*, 6> MAGIC_OPTIONS = {{
    "None (No magic exists)",
    "Mythical (Legends only)",
    "Rare (Few practitioners)",
    "Uncommon (Known but limited)",
    "Common (Widely practiced)",
    "Ubiquitous (Everyday occurrence)",
}};

inline constexpr std::array<const char*, 6> SPECIES_OPTIONS = {{
    "Human only",
    "Human majority",
    "Multiple species (diverse)",
    "Non-human majority",
    "Mixed races (equal)",
    "Unknown/Mysterious",
}};

inline constexpr std::array<const char*, 14> THREAT_OPTIONS = {{
    "Wildlife predators",
    "Hostile tribes",
    "Bandits and raiders",
    "Political conflict",
    "Natural disasters",
    "Mysterious forces",
    "Ancient evils",
    "Resource scarcity",
    "Corporate espionage",
    "Rogue AI",
    "Cyber criminals",
    "Government surveillance",
    "Gang warfare",
    "Environmental collapse",
}};

} // namespace oce
