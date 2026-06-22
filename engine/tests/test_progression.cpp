// Deterministic tests for dice, the attribute modifier, the XP curve, and
// level-up. Note: floor(100 * level^1.5) yields 282 and 519 at levels 2 and 3
// (not the 283 and 520 that rounding to a whole number might suggest); these
// tests assert the true computed values.
#include "oce/rules/dice.hpp"
#include "oce/rules/leveling.hpp"

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
    // Attribute modifier: floor((value - 10) / 2).
    CHECK(oce::modifier(10) == 0);
    CHECK(oce::modifier(11) == 0);
    CHECK(oce::modifier(12) == 1);
    CHECK(oce::modifier(8) == -1);
    CHECK(oce::modifier(5) == -3);
    CHECK(oce::modifier(3) == -4);
    CHECK(oce::modifier(1) == -5);
    CHECK(oce::modifier(20) == 5);

    // Dice: same seed reproduces the sequence; rolls stay in range.
    oce::Rng a(42u);
    oce::Rng b(42u);
    for (int i = 0; i < 200; ++i) {
        int ra = a.roll(2, 6);
        int rb = b.roll(2, 6);
        CHECK(ra == rb);
        CHECK(ra >= 2 && ra <= 12);
    }
    oce::Rng c(7u);
    for (int i = 0; i < 200; ++i) {
        int r = c.roll(1, 6);
        CHECK(r >= 1 && r <= 6);
    }

    // XP curve: the true floor(100*level^1.5) values.
    CHECK(oce::xp_for_next_level(1) == 100);
    CHECK(oce::xp_for_next_level(2) == 282);
    CHECK(oce::xp_for_next_level(3) == 519);
    CHECK(oce::xp_for_next_level(5) == 1118);
    CHECK(oce::xp_for_next_level(10) == 3162);
    CHECK(oce::xp_for_next_level(20) == 8944);

    // Level-up: enough XP for exactly two levels from level 1.
    oce::Player p;
    p.xp = oce::xp_for_next_level(1) + oce::xp_for_next_level(2); // 100 + 282
    int gained = oce::apply_level_up(p);
    CHECK(gained == 2);
    CHECK(p.level == 3);
    CHECK(p.xp == 0);
    CHECK(p.max_hp == 70);        // 50 + 2*10
    CHECK(p.max_energy == 30);    // 20 + 2*5
    CHECK(p.attribute_points == 6); // 2*3
    CHECK(p.hp == 70);
    CHECK(p.energy == 30);

    // Below the threshold: no level gained.
    oce::Player q;
    q.xp = 99;
    CHECK(oce::apply_level_up(q) == 0);
    CHECK(q.level == 1 && q.xp == 99);

    if (failures == 0) {
        printf("progression: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "progression: %d checks failed\n", failures);
    return 1;
}
