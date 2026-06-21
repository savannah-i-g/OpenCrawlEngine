# OpenCrawlEngine

A local, single-player RPG engine with a language-model game master. You bring an
OpenAI-compatible API key; the engine runs on your machine, stores your
characters and campaigns in a local SQLite database, and renders a desktop
interface. No server, no account, no telemetry.

> **Status:** early development. The build and application scaffold are in place;
> gameplay systems land milestone by milestone (see `CHANGELOG.md`).

## Features

- Language-model game master that narrates and drives the world through
  structured tool calls.
- Deterministic rules engine: attributes, items, turn-based combat, skill
  checks, and leveling.
- Characters and campaigns persisted locally in SQLite.
- Desktop UI built with Dear ImGui.
- Bring-your-own-key: works with any OpenAI-compatible endpoint.

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
