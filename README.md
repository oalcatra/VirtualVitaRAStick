# VirtualVitaRAStick

Experimental PS Vita taiHEN kernel plugin project for front-touch virtual right-analog control.

## Current milestone: Phase 0

The current code is intentionally a minimal kernel plugin. It does **not** hook touch or controller input yet. Its purpose is to validate the VitaSDK/GitHub Actions build pipeline and produce a loadable `.skprx` before input hooks are introduced.

## Roadmap

- Phase 0: minimal `.skprx` builds successfully in CI.
- Phase 1: read front touchscreen and map a touch zone to right-stick X/Y.
- Phase 2: deadzone, radius and configurable touch region.
- Phase 3: visible virtual-stick overlay.
- Phase 4: optional virtual buttons and per-game profiles.

## Safety

This is experimental kernel-level software. Do not install development builds on a Vita until the relevant build has been reviewed and marked ready for hardware testing. Keep a backup of `ur0:tai/config.txt` before testing kernel plugins.
