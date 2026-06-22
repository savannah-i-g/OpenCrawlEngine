# Changelog

All notable changes to this project are documented in this file. The format is
based on [Keep a Changelog](https://keepachangelog.com/), and the project
follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- CMake build system (C11 + C++17) with UI and headless configurations, strict
  warnings, and AddressSanitizer/UBSan in Debug builds.
- Dear ImGui (docking) desktop frontend that opens an application window.
- Engine library skeleton with a unit-test harness wired into CTest.
- Infrastructure libraries (C11): JSON over cJSON, an in-memory secret store
  with secure zeroization, an HTTP transport with a Server-Sent Events parser,
  an OpenAI-compatible streaming chat client with tool-call assembly, a
  single-agent turn loop with a tool registry, and SQLite/in-memory persistence.
- A headless game engine driven by a language-model game master through tool
  calls (apply stat changes, set suggested actions), with a background turn
  worker and local save/load.
- A game interface: a story log with live streaming narrative, a command input,
  suggested-action buttons, character status, and in-app API-key entry.
- The full game model: ten attributes, six character classes, items with
  rarities and effects, equipment, enemies, combat and skill-check state, NPCs,
  businesses, relations, properties, mounts, factions, and world state.
- Deterministic rules driven by a seedable RNG — dice and attribute modifiers,
  the XP curve and level-ups, turn-based combat (attack/defend/flee, enemy
  derivation, rewards), skill checks and the spellcasting gate, item
  generation/equip/consume, the per-class character factory, and asset/world
  bookkeeping — each covered by unit tests.
- The game master can now adjust energy and experience (with automatic
  level-ups) alongside health and gold; the interface shows class, level, and
  energy.
- A full game-master tool suite: start_combat/end_combat, set_skill_check,
  add_item/remove_item/equip_item/unequip_item,
  add_business/add_relation/add_property/add_mount/change_faction, and
  upsert_npc/set_location/add_world_fact, alongside apply_stat_changes and
  set_suggested_actions. Tools dispatch through one registry under the state
  lock; every tool validates its arguments and returns a structured result, and
  each is unit-tested including malformed input. Each turn the model receives a
  compact JSON view of the game state.
- Interactive, engine-resolved combat: attack/defend/flee with deterministic
  enemy turns, per-enemy xp/gold rewards (and leveling) as foes fall, and
  victory/defeat/flee resolution with a running combat log. A combat panel
  surfaces enemies, the log, and the available actions.
- Interactive skill checks: when the game master sets a check, the player rolls
  (2d6/4d6 + attribute modifier vs difficulty) and the success or failure branch
  is recorded to the story; a skill-check panel drives it.
- New game and character creation: choose a name, class, background, and a
  free-text setting; the engine builds the character with its class attributes
  and a starting kit, resets state, and begins a fresh game-master conversation
  that builds on the setting. A New Game dialog drives it.
- Assets and world bookkeeping: businesses accrue daily gold over elapsed game
  time, and the full game state (inventory, equipment, businesses, relations,
  properties, mounts, factions, NPCs, world facts, combat, and skill checks)
  serializes to and from the save (round-trip tested). An Assets dialog lists
  holdings, factions, and known NPCs.
- Multiple saved games: each New Game is its own campaign; a Load Game dialog
  lists saves and switches between them; the active campaign is remembered and
  resumes on restart, re-seeding recent story into a fresh game-master
  conversation so context carries over. The save format carries a version field
  for future migration.
