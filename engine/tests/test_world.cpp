// Deterministic tests for asset accrual, relationship clamps, and world state.
#include "oce/rules/world.hpp"

#include <cstdio>

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

    // Business income accrues per whole day; partial days carry over.
    Business b;
    b.income_per_day = 10;
    b.last_collected = 0;
    CHECK(collect_business_income(b, kMinutesPerDay) == 10); // exactly one day
    CHECK(b.last_collected == kMinutesPerDay);
    CHECK(collect_business_income(b, kMinutesPerDay) == 0); // nothing new
    CHECK(collect_business_income(b, kMinutesPerDay + 2 * kMinutesPerDay) == 20); // two more days
    CHECK(collect_business_income(b, b.last_collected + 700) == 0); // partial day -> nothing
    Business idle;
    idle.income_per_day = 0;
    CHECK(collect_business_income(idle, 10 * kMinutesPerDay) == 0);

    // Faction standing clamps.
    Faction f;
    f.relationship = 50;
    f.reputation = 500;
    change_faction(f, 60, 600);
    CHECK(f.relationship == 100 && f.reputation == 1000);
    change_faction(f, -300, -2000);
    CHECK(f.relationship == -100 && f.reputation == 0);

    // Relation strength and mount condition clamps.
    Relation r;
    r.strength = 90;
    change_relation_strength(r, 20);
    CHECK(r.strength == 100);
    change_relation_strength(r, -300);
    CHECK(r.strength == -100);

    MountVehicle m;
    m.condition = 50;
    change_mount_condition(m, 60);
    CHECK(m.condition == 100);
    change_mount_condition(m, -200);
    CHECK(m.condition == 0);

    // World-state transitions.
    WorldState w;
    set_location(w, "Town");
    CHECK(w.current_location == "Town");
    CHECK(w.visited_locations.size() == 1u);
    set_location(w, "Town"); // revisiting does not duplicate
    CHECK(w.visited_locations.size() == 1u);
    set_location(w, "Cave");
    CHECK(w.current_location == "Cave" && w.visited_locations.size() == 2u);

    advance_time(w, 30);
    CHECK(w.time_elapsed == 30);
    advance_time(w, 0);
    advance_time(w, -5); // non-positive is ignored
    CHECK(w.time_elapsed == 30);

    add_world_fact(w, "The bridge is out.");
    add_world_fact(w, "The bridge is out."); // no duplicate
    add_world_fact(w, "A storm is coming.");
    CHECK(w.world_facts.size() == 2u);

    NPC npc;
    npc.id = "n1";
    npc.name = "Bob";
    upsert_npc(w, npc);
    CHECK(w.known_npcs.size() == 1u && w.known_npcs["n1"].name == "Bob");
    npc.name = "Bobby";
    upsert_npc(w, npc); // same id updates in place
    CHECK(w.known_npcs.size() == 1u && w.known_npcs["n1"].name == "Bobby");

    Faction g;
    g.id = "guild";
    g.name = "Merchants";
    upsert_faction(w, g);
    CHECK(w.factions.size() == 1u && w.factions["guild"].name == "Merchants");

    if (failures == 0) {
        printf("world: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "world: %d checks failed\n", failures);
    return 1;
}
