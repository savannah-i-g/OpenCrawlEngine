#include "oce/rules/mounts.hpp"

#include <utility>

namespace oce {

namespace {

MountVehicle mk(const char* type, const char* name, const char* desc, double speed, int capacity,
                int upkeep, std::vector<std::string> abilities) {
    MountVehicle m;
    m.type = type;
    m.name = name;
    m.description = desc;
    m.speed = speed;
    m.capacity = capacity;
    m.upkeep_cost = upkeep;
    m.special_abilities = std::move(abilities);
    return m;
}

const MountVehicle kDonkey = mk("donkey", "Pack Donkey",
                                "A sturdy beast of burden for carrying supplies", 1.2, 10, 1, {});
const MountVehicle kHorse = mk("horse", "Riding Horse", "A reliable mount for faster travel", 1.5,
                               5, 2, {});
const MountVehicle kWarhorse = mk("warhorse", "Warhorse",
                                  "A trained battle mount with combat bonuses", 1.4, 3, 5,
                                  {"combat_bonus"});
const MountVehicle kCarriage = mk("carriage", "Horse-Drawn Carriage",
                                  "A comfortable enclosed vehicle for travel", 1.3, 15, 3, {});

} // namespace

std::vector<MountVehicle> available_mounts(const std::string& technology, const std::string& magic) {
    std::vector<MountVehicle> mounts;

    if (technology == "Stone Age (Primitive tools)") {
        mounts = {kDonkey};
    } else if (technology == "Bronze Age (Early metalworking)") {
        mounts = {kDonkey, kHorse, mk("cart", "Ox Cart", "A simple wheeled cart pulled by oxen", 1.1,
                                      20, 2, {})};
    } else if (technology == "Iron Age (Advanced metalworking)") {
        mounts = {kHorse, kWarhorse,
                  mk("camel", "Desert Camel", "Perfect for arid climates, requires less water", 1.3,
                     8, 2, {})};
    } else if (technology == "Renaissance (Early modern)") {
        mounts = {kHorse, kCarriage,
                  mk("ship", "Sailing Ship", "A seafaring vessel for ocean travel", 2.0, 50, 10,
                     {"water_travel"})};
    } else if (technology == "Industrial (Steam power)") {
        mounts = {mk("train", "Steam Train", "A powerful locomotive for rapid overland travel", 3.0,
                     100, 20, {}),
                  mk("automobile", "Automobile", "A self-propelled vehicle for personal transport",
                     2.5, 10, 5, {}),
                  mk("airship", "Airship", "A lighter-than-air craft for aerial travel", 2.8, 30, 15,
                     {"flight"})};
    } else if (technology == "Modern (Present day)") {
        mounts = {mk("automobile", "Compact Car", "A reliable everyday vehicle", 3.0, 8, 5, {}),
                  mk("automobile", "SUV", "A rugged vehicle with all-terrain capability", 2.8, 15, 8,
                     {"terrain_bonus"}),
                  mk("motorcycle", "Motorcycle", "A fast and agile two-wheeled vehicle", 3.5, 3, 3,
                     {}),
                  mk("ship", "Speedboat", "A fast watercraft for coastal travel", 3.0, 10, 10,
                     {"water_travel"}),
                  mk("helicopter", "Helicopter", "A versatile aircraft for aerial transport", 4.0, 8,
                     50, {"flight"})};
    } else if (technology == "Cyberpunk (Near future)") {
        mounts = {mk("automobile", "Electric Car", "A sleek autonomous electric vehicle", 3.5, 8, 4,
                     {"stealth_mode"}),
                  mk("motorcycle", "Cyberbike", "A high-tech motorcycle with a neural interface", 4.5,
                     3, 6, {"combat_bonus"}),
                  mk("hoverbike", "Hoverbike", "A compact anti-gravity vehicle for urban navigation",
                     4.0, 5, 10, {"flight"}),
                  mk("drone", "Personal Drone", "An automated flying carrier", 3.0, 10, 5,
                     {"flight", "stealth_mode"}),
                  mk("airship", "VTOL Craft", "A vertical take-off aircraft for rapid deployment",
                     5.0, 20, 30, {"flight"})};
    } else if (technology == "Mixed (Varied by region)") {
        mounts = {kHorse, kCarriage};
    } else {
        // Stone-/medieval-era and any unrecognized level default to the feudal roster.
        mounts = {kHorse, kWarhorse, kCarriage};
    }

    if (magic == "Common (Widely practiced)" || magic == "Ubiquitous (Everyday occurrence)") {
        mounts.push_back(mk("dragon", "Young Dragon", "A majestic flying mount with immense power",
                            3.0, 20, 50, {"flight", "combat_bonus"}));
        mounts.push_back(mk("other", "Giant Eagle", "A noble bird of prey, large enough to ride",
                            2.5, 5, 10, {"flight"}));
        mounts.push_back(mk("other", "Unicorn", "A mystical steed blessed with magic", 2.0, 3, 5,
                            {"magic_detection"}));
    }

    return mounts;
}

} // namespace oce
