#pragma once
// The game model. Plain value types; serialization and rules live elsewhere.

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oce {

enum class CharacterClass { Warrior, Rogue, Mage, Cleric, Ranger, Bard };
enum class ItemRarity { Common, Uncommon, Rare, Epic, Legendary };
enum class ItemKind { Weapon, Armor, Potion };
enum class ItemSlot { Hand, Body, Consumable };

// The ten character attributes. Defaults match a fresh, unallocated character.
struct Attributes {
    int strength = 5;
    int dexterity = 5;
    int intelligence = 5;
    int constitution = 5;
    int wisdom = 5;
    int charisma = 5;
    int luck = 5;
    int perception = 5;
    int stealth = 5;
    int bartering = 5;
};

// Item bonuses. A zero field means "no effect"; hp/energy are restore amounts.
struct ItemEffects {
    int strength = 0;
    int dexterity = 0;
    int intelligence = 0;
    int constitution = 0;
    int wisdom = 0;
    int charisma = 0;
    int luck = 0;
    int perception = 0;
    int stealth = 0;
    int bartering = 0;
    int hp = 0;
    int energy = 0;
};

struct Item {
    std::string id;
    std::string name;
    std::string description;
    ItemKind kind = ItemKind::Weapon;
    ItemRarity rarity = ItemRarity::Common;
    ItemSlot slot = ItemSlot::Hand;
    ItemEffects effects;
    std::string icon;
};

struct Equipment {
    std::optional<Item> hand;
    std::optional<Item> body;
};

struct Player {
    std::string name = "Adventurer";
    CharacterClass cls = CharacterClass::Warrior;
    int level = 1;
    long long xp = 0;
    int gold = 50;
    int hp = 50;
    int max_hp = 50;
    int energy = 20;
    int max_energy = 20;
    Attributes attributes;
    int attribute_points = 0;
    std::string background;
};

struct Enemy {
    std::string id;
    std::string name;
    std::string description;
    int hp = 0;
    int max_hp = 0;
    int attack = 0;
    int defense = 0;
};

struct Message {
    std::string sender; // "narrator" | "player" | "system"
    std::string content;
    long long ts = 0;
};

struct CombatState {
    bool active = false;
    std::vector<Enemy> enemies;
    std::string turn = "player"; // "player" or an enemy id
    std::vector<std::string> log;
    int player_guard = 0; // transient defensive bonus carried into the enemy phase
};

struct SkillCheck {
    bool active = false;
    std::string attribute = "strength";
    int difficulty = 10;
    int num_dice = 2; // 2 or 4
    std::string description;
    std::string on_success;
    std::string on_failure;
};

// The most recent resolved dice roll (skill check or combat attack), surfaced to
// the UI's roll panels. Transient: not serialized.
struct DiceRoll {
    std::string name;        // e.g. "Stealth" or "Attack: Goblin (DEF 7)"
    std::vector<int> dice;   // individual d6 results
    int modifier = 0;
    int total = 0;           // sum(dice) + modifier
    int target = 0;          // DC or defense to beat
    bool success = false;
    long long seq = 0;       // increments per roll; 0 means none yet
};

struct NPC {
    std::string id;
    std::string name;
    std::string description;
    std::string location;
    int relationship = 0; // -100..100
    std::string occupation;
    std::string last_dialogue;
};

enum class BusinessType {
    Tavern,
    Shop,
    Farm,
    Mine,
    TradingCompany,
    MercenaryGuild,
    Workshop,
    Other
};

struct Business {
    std::string id;
    std::string name;
    BusinessType type = BusinessType::Other;
    std::string description;
    std::string location;
    int value = 0;
    int income_per_day = 0;
    long long last_collected = 0; // game-time minutes at last collection
    int employee_count = 0;
    std::string manager_id;
};

struct Relation {
    std::string id;
    std::string npc_id;
    std::string npc_name;
    std::string type; // ally/friend/family/rival/enemy/mentor/student/business_partner
    int strength = 0;  // -100..100
    std::string description;
    std::vector<std::string> benefits;
};

struct Property {
    std::string id;
    std::string name;
    std::string type; // house/estate/castle/hideout/tower/ship/other
    std::string location;
    std::string description;
    int value = 0;
    std::vector<std::string> provides;
};

struct MountVehicle {
    std::string id;
    std::string name;
    std::string type;
    std::string description;
    std::string era;
    double speed = 1.0;
    int capacity = 0;
    int condition = 100; // 0..100
    int upkeep_cost = 0;
    std::vector<std::string> special_abilities;
};

enum class FactionType {
    Guild,
    Kingdom,
    Clan,
    Cult,
    MerchantCompany,
    Military,
    Religious,
    Criminal,
    Other
};

struct Faction {
    std::string id;
    std::string name;
    std::string description;
    FactionType type = FactionType::Other;
    int relationship = 0; // -100..100
    int reputation = 0;   // 0..1000
    std::string territory;
    std::string leader;
    std::vector<std::string> benefits;
    bool discovered = false;
    std::string last_interaction;
};

struct PlayerAssets {
    std::vector<Business> businesses;
    std::vector<Relation> relations;
    std::vector<Property> properties;
    std::vector<MountVehicle> mounts;
};

struct WorldState {
    std::string current_location = "Unknown";
    std::vector<std::string> visited_locations;
    std::map<std::string, NPC> known_npcs;
    std::map<std::string, Faction> factions;
    std::vector<std::string> world_facts;
    long long time_elapsed = 0; // minutes
    std::string technology;     // world technology level (drives the mount roster)
    std::string magic;          // world magic level (gates fantasy mounts)
};

enum class CombatOutcomeType { Victory, Defeat, Fled };

struct CombatOutcome {
    CombatOutcomeType type = CombatOutcomeType::Victory;
    std::vector<Enemy> defeated_enemies;
    long long xp_gained = 0;
    int gold_gained = 0;
};

enum class Difficulty { Easy, Normal, Hard, Deadly };

// Per-campaign metadata (the campaign half of a save; the rest of GameState
// above the line — player/inventory/equipment/assets/world — is the persistent
// character half).
struct CampaignMeta {
    std::string name = "Adventure";
    std::string theme;
    std::string tone;
    std::vector<std::string> goals;
    Difficulty difficulty = Difficulty::Normal;
    std::string custom_prompt; // extra game-master directive
    std::vector<std::string> tags;
    std::string notes;
};

struct GameState {
    Player player;
    std::vector<Item> inventory;
    Equipment equipment;
    PlayerAssets assets;
    std::vector<Message> story;
    std::string world_description;
    std::string world_context;
    WorldState world_state;
    CombatState combat;
    SkillCheck skill_check;
    std::vector<std::string> suggested_actions;
    CampaignMeta meta;
};

} // namespace oce
