# OpenCrawlEngine

A local, single-player RPG engine with a language-model game master. You bring an
OpenAI-compatible API key; the engine runs on your machine, stores your
characters and campaigns in a local SQLite database, and renders a desktop
interface. No server, no account, no telemetry.

> **Status:** feature-complete for single-player play — character creation, a
> tool-driven game master, turn-based combat, skill checks, inventory, assets,
> and local saves. Bring an OpenRouter key to play. See `CHANGELOG.md`.

## Features

- A language-model game master that narrates and drives the world through
  structured tool calls.
- Create a character (six classes, attributes, a background) in a world of your
  own description.
- A deterministic rules engine: attributes, items and equipment, turn-based
  combat, skill checks, leveling, businesses, factions, and world state.
- Multiple campaigns saved locally in SQLite, resuming where you left off.
- A Dear ImGui desktop interface; choose your model and endpoint in settings.
- Bring-your-own-key: works with any OpenAI-compatible endpoint (e.g. OpenRouter).

## Building

Prerequisites (Debian / Ubuntu):

```
sudo apt install build-essential cmake ninja-build pkg-config \
    libglfw3-dev libgl1-mesa-dev libcurl4-openssl-dev libcjson-dev libsqlite3-dev
```

Build and run:

```
cmake -S . -B build -G Ninja
cmake --build build
./build/bin/OpenCrawlEngine
```

Dear ImGui is fetched automatically at configure time; the remaining
dependencies are taken from the system.

## Configuration

Provide an OpenAI-compatible API key through the environment (or enter it in the
application's settings):

```
export OPENROUTER_API_KEY=sk-...
```

## License

MIT — see [`LICENSE`](LICENSE).
