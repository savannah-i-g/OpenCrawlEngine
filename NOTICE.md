# Third-Party Notices

OpenCrawlEngine is released under the MIT License (see `LICENSE`). It builds on
the following third-party components, each under its own license:

- **Dear ImGui** — MIT License. Copyright (c) 2014-2025 Omar Cornut.
  <https://github.com/ocornut/imgui>
- **cJSON** — MIT License. Copyright (c) 2009-2017 Dave Gamble and cJSON
  contributors. <https://github.com/DaveGamble/cJSON>
- **libcurl** — curl License (an MIT/X11 derivative). Copyright (c) 1996-2025
  Daniel Stenberg and contributors. <https://curl.se/docs/copyright.html>
- **SQLite** — Public Domain. <https://www.sqlite.org/copyright.html>
- **GLFW** — zlib/libpng License. Copyright (c) 2002-2006 Marcus Geelnard,
  2006-2019 Camilla Löwy. <https://www.glfw.org/license.html>
- **nanosvg** — zlib License. Copyright (c) 2013-14 Mikko Mononen. Used to
  rasterize the bundled icons. <https://github.com/memononen/nanosvg>

## Icons

The frontend bundles a curated subset of icons from **game-icons.net**, licensed
under the **Creative Commons Attribution 3.0 Unported (CC BY 3.0)** license
(<https://creativecommons.org/licenses/by/3.0/>). The icons are stored under
`app/ui/assets/icons/` and were authored by the game-icons.net contributors,
including (among others) Lorc, Delapouite, Faithtoken, Skoll, Sbed,
Cathelineau, Carl Olsen, and DarkZaitzev. Full per-icon attribution and the
original artwork are available at <https://game-icons.net>.

## Fonts

The frontend loads a serif text font from the host system at runtime when one is
available, falling back to Dear ImGui's built-in font; no font binaries are
bundled in this repository.
