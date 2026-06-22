// Regression: resolving a skill check auto-continues a narration turn, and the
// game master must not be able to immediately re-arm another check there — that
// previously looped (resolve -> narrate -> re-request -> resolve ...). A check
// armed on an ordinary turn still sticks; one armed on a resolution turn is
// dropped, and the suppression is one-shot.
#include "oce/engine.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

// Every turn arms a skill check on its first call (modelling a model that keeps
// asking for checks) and narrates on the second, once tool results are present.
extern "C" int loop_mock(void* ctx, const oce_llm_message* msgs, size_t n, const char* tools_json,
                         const oce_llm_handlers* h, char* fr, size_t cap) {
    (void) ctx;
    (void) tools_json;
    // The persistent turn agent accumulates history across turns, so a tool
    // message merely *existing* is not a per-turn signal. The phase within this
    // turn is told by the last message: a tool result means we already issued
    // our tool calls and should now narrate; otherwise it is the turn's first
    // call and we arm a check.
    const bool just_ran_tools =
        n > 0 && msgs[n - 1].role != nullptr && std::strcmp(msgs[n - 1].role, "tool") == 0;
    if (!just_ran_tools) {
        h->on_tool_call("c1", "set_skill_check", "{\"attribute\":\"strength\",\"difficulty\":10}",
                        h->user);
        h->on_tool_call("s1", "set_suggested_actions", "{\"actions\":[\"Press on\",\"Rest\"]}",
                        h->user);
        std::snprintf(fr, cap, "tool_calls");
    } else {
        const char* t = "The moment passes and the scene shifts.";
        h->on_text(t, std::strlen(t), h->user);
        std::snprintf(fr, cap, "stop");
    }
    return OCE_AGENT_BACKEND_OK;
}

int main(void) {
    oce_agent_backend backend = {loop_mock, nullptr};

    oce::EngineConfig cfg;
    cfg.store_backend = OCE_STORE_MEMORY;
    cfg.test_backend = &backend;
    oce::Engine engine(cfg);

    oce::NewGameParams p;
    p.name = "Tess";
    p.cls = oce::CharacterClass::Rogue;
    engine.new_game(p);

    // A check armed during an ordinary player turn sticks.
    engine.submit_turn("I shoulder the heavy door.");
    engine.wait_idle();
    CHECK(engine.state_copy().skill_check.active);

    // Resolving it auto-continues a narration turn; the check the game master
    // re-arms there must be suppressed (no loop).
    engine.resolve_skill_check();
    engine.wait_idle();
    CHECK(!engine.state_copy().skill_check.active);

    // The suppression is one-shot: the next ordinary turn can arm a check again.
    engine.submit_turn("I search the next room.");
    engine.wait_idle();
    CHECK(engine.state_copy().skill_check.active);

    if (failures == 0) {
        printf("skill_loop: all checks passed\n");
        return 0;
    }
    fprintf(stderr, "skill_loop: %d checks failed\n", failures);
    return 1;
}
