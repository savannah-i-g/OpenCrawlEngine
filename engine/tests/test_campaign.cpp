// Deterministic tests for the character/campaign serialization split, the
// campaign difficulty enum, and difficulty scaling offsets.
#include "oce/model.hpp"
#include "oce/rules/character.hpp"
#include "oce/rules/skills.hpp"
#include "oce/serialize.hpp"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

int main(void) {
    using namespace oce;

    // Difficulty <-> string round-trips; an unknown label is rejected.
    {
        CHECK(std::string(difficulty_to_string(Difficulty::Easy)) == "easy");
        CHECK(std::string(difficulty_to_string(Difficulty::Normal)) == "normal");
        CHECK(std::string(difficulty_to_string(Difficulty::Hard)) == "hard");
        CHECK(std::string(difficulty_to_string(Difficulty::Deadly)) == "deadly");
        Difficulty d = Difficulty::Normal;
        CHECK(difficulty_from_string("deadly", d) && d == Difficulty::Deadly);
        CHECK(difficulty_from_string("easy", d) && d == Difficulty::Easy);
        CHECK(!difficulty_from_string("brutal", d));
    }

    // Difficulty offsets shift skill-check DCs and enemy levels.
    {
        CHECK(difficulty_dc_offset(Difficulty::Easy) == -2);
        CHECK(difficulty_dc_offset(Difficulty::Normal) == 0);
        CHECK(difficulty_dc_offset(Difficulty::Hard) == 2);
        CHECK(difficulty_dc_offset(Difficulty::Deadly) == 4);
        CHECK(difficulty_level_offset(Difficulty::Easy) == -1);
        CHECK(difficulty_level_offset(Difficulty::Normal) == 0);
        CHECK(difficulty_level_offset(Difficulty::Hard) == 1);
        CHECK(difficulty_level_offset(Difficulty::Deadly) == 2);
    }

    // Build a state spanning both halves.
    GameState s;
    s.player.name = "Mara";
    Item potion;
    potion.id = "potion";
    potion.name = "Healing Potion";
    s.inventory.push_back(potion);
    s.world_state.current_location = "Ashford";
    s.world_state.world_facts = {"The bridge is out"};
    s.story.push_back(Message{"narrator", "You arrive.", 0});
    s.suggested_actions = {"Go north"};
    s.meta.name = "The Long Road";
    s.meta.theme = "mystery";
    s.meta.difficulty = Difficulty::Hard;
    s.meta.goals = {"Find the relic"};

    // The character half carries persistent state and omits the campaign half.
    {
        const std::string json = serialize_character(s);
        GameState a;
        deserialize_character(json.c_str(), a);
        CHECK(a.player.name == "Mara");
        CHECK(a.inventory.size() == 1u && a.inventory[0].id == "potion");
        CHECK(a.world_state.current_location == "Ashford");
        CHECK(a.world_state.world_facts.size() == 1u);
        CHECK(a.story.empty());                  // campaign-only
        CHECK(a.suggested_actions.empty());      // campaign-only
        CHECK(a.meta.name == "Adventure");        // default; campaign-only
    }

    // The campaign half carries per-adventure state and omits the character half.
    {
        const std::string json = serialize_campaign(s);
        GameState b;
        deserialize_campaign(json.c_str(), b);
        CHECK(b.story.size() == 1u && b.story[0].content == "You arrive.");
        CHECK(b.suggested_actions.size() == 1u);
        CHECK(b.meta.name == "The Long Road");
        CHECK(b.meta.theme == "mystery");
        CHECK(b.meta.difficulty == Difficulty::Hard);
        CHECK(b.meta.goals.size() == 1u && b.meta.goals[0] == "Find the relic");
        CHECK(b.player.name == "Adventurer");                 // default; character-only
        CHECK(b.inventory.empty());                           // character-only
        CHECK(b.world_state.current_location == "Unknown");   // default; character-only
    }

    if (failures == 0) {
        printf("campaign: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "campaign: %d checks failed\n", failures);
    return 1;
}
