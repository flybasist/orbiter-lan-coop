**English** · [Русский](ru/CREDITS.md)

# Third-party components

orbiter-lan-coop is distributed under the GPLv3 license. Below is everything used from other
people's work and on what terms.

## Orbiter SDK

- **What it is:** headers and libraries for Orbiter add-ons (`Orbitersdk/include`,
  `orbiter.lib`, `Orbitersdk.lib`).
- **Where from:** https://github.com/orbitersim/orbiter, base commit `a7a3396e`.
- **Terms:** MIT License, © Martin Schweiger. `Orbitersdk.lib` is static and its code goes into
  the plugin DLL, so a release includes the text of Orbiter's MIT license with the copyright
  notice. `Orbitersdk.lib` also contains Dear ImGui (MIT, © Omar Cornut) and
  ImPlot (MIT, © Evan Pezent) — their notices go into a release the same way.
- **In the repository:** no; the GitHub Actions build compiles it from the base commit.

## Orbiter

- **What it is:** the space flight simulator itself, in which the plugin runs.
- **Where from:** https://github.com/orbitersim/orbiter.
- **Terms:** MIT License (the D3D9Client graphics client is GPLv3/LGPLv3). OLC does not
  link with it, it only runs inside it.
- **In the repository:** no.
