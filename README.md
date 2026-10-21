# VirtualVitaRAStick

A lightweight taiHEN kernel plugin for PlayStation Vita that turns the **right half of the front touchscreen into a virtual right analog stick**.

VirtualVitaRAStick is intended especially for systems with a **broken, drifting, or physically missing right analog stick**. The plugin takes control of the right-stick axes and keeps them centered when the virtual stick is not being used, preventing unwanted movement from faulty hardware.

## Version 1.0

The current build is fully usable and has been tested on real hardware.

### Features

- Front touchscreen acts as a floating right analog stick.
- Touch anywhere on the right half of the screen to establish the temporary stick center.
- Move your finger around that point to control the right analog axes.
- Release the screen and the stick immediately returns to neutral (`128 / 128`).
- Physical right-stick input is overridden, eliminating drift/noise from a broken or missing stick.
- Tuned virtual-stick radius for responsive movement.
- Works alongside reVita in the tested setup.
- No configuration app or additional runtime dependency beyond a normal taiHEN/enso plugin setup.

## Tested hardware

Version 1.0 has been tested successfully on a **PS Vita PCH-1004 (Fat)** with the physical right analog stick absent.

The plugin has also been tested running alongside **reVita**. Other models, games and plugin combinations may work, but have not all been individually verified yet.

## Installation

1. Download `VirtualVitaRAStick.skprx` from the latest release.
2. Copy it to:

   ```text
   ur0:tai/VirtualVitaRAStick.skprx
   ```

3. Open `ur0:tai/config.txt` and add the following line under the existing `*KERNEL` section:

   ```text
   ur0:tai/VirtualVitaRAStick.skprx
   ```

4. Save the file and reboot the PS Vita.

Do **not** create a second `*KERNEL` section if one already exists.

## How it works

VirtualVitaRAStick hooks the Vita controller input path and reads the front touchscreen. A touch beginning on the right half of the front panel becomes the center of a floating analog stick.

While the touch is active, finger displacement is converted into right-stick X/Y values. When no qualifying touch is present, the plugin forces the right stick to its neutral position.

This means a disconnected or faulty physical right analog stick cannot continuously inject unwanted movement through the controller APIs currently handled by the plugin.

## Current behavior

- Touch region: right half of the front touchscreen.
- Stick style: floating center.
- Virtual radius: `180` touch-coordinate units.
- Idle right stick: forced neutral.
- Physical right analog: intentionally ignored.

## Compatibility notes

VirtualVitaRAStick calls the previous controller hook first and modifies only the right analog axes afterwards. This allows it to coexist with reVita in the configuration used during hardware testing while leaving buttons, D-pad and left-stick data untouched.

Because VirtualVitaRAStick intentionally owns the right analog axes, another plugin attempting to modify those same axes may have its right-stick output overridden.

## Recovery / safety

This is a kernel plugin. Keep a backup of `ur0:tai/config.txt` before changing your taiHEN configuration.

If a plugin configuration prevents the Vita from booting normally, hold **L while booting** to skip taiHEN plugins, then remove or comment out the VirtualVitaRAStick entry from `config.txt`.

## Building from source

The repository includes a GitHub Actions workflow that builds the plugin with VitaSDK. Every push to the main development flow can produce a compiled `.skprx` artifact without requiring a local VitaSDK installation.

For local development, a working VitaSDK environment is required.

## Roadmap

Possible future improvements include:

- Selectable left/right virtual analog stick.
- Configurable sensitivity and radius.
- Configurable touch zones.
- Optional deadzone controls.
- On-screen virtual-stick feedback.
- Optional virtual buttons.
- Per-game profiles.
- Broader controller API coverage and compatibility testing.

## Credits

Developed by **Alcatra**.

The PS Vita homebrew ecosystem, VitaSDK/taiHEN documentation and open-source projects such as reVita were valuable technical references while researching Vita input hooks and plugin interoperability.

## Status

**v1.0 — hardware tested and usable.**

The first public version focuses on doing one thing well: replacing an unusable physical right analog stick with a simple front-touch virtual stick.
