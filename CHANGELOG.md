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
