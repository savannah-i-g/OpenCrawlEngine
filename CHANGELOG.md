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
