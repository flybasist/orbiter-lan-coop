**English** · [Русский](ru/CREDITS.md)

# Third-party components

orbiter-lan-coop is distributed under the GPLv3 license. Below is everything used from other
people's work and on what terms.

## Orbiter SDK

- **What it is:** headers and libraries for Orbiter add-ons (`Orbitersdk/include`,
  `orbiter.lib`, `Orbitersdk.lib`).
- **Where from:** https://github.com/orbitersim/orbiter, base commit `48c30c77`.
- **Terms:** MIT License, © Martin Schweiger. `Orbitersdk.lib` is static and its code goes into
  the plugin DLL. `Orbitersdk.lib` also contains Dear ImGui (MIT, © Omar Cornut) and ImPlot
  (MIT, © Evan Pezent). The texts of all three licenses are in `tools/package/THIRD-PARTY-NOTICES.txt`
  and go next to the DLL into the plugin archive (`OLC-THIRD-PARTY-NOTICES.txt`).
- **In the repository:** no; the GitHub Actions build compiles it from the base commit.

## Orbiter

- **What it is:** the space flight simulator itself, in which the plugin runs.
- **Where from:** https://github.com/orbitersim/orbiter.
- **Terms:** MIT License (the D3D9Client graphics client is GPLv3/LGPLv3). OLC does not
  link with it, it only runs inside it.
- **In the repository:** no. **In the test package:** yes — `olc-1-orbiter.zip` (with Orbiter's
  `LICENSE` file).

## SPICE kernels

- **What it is:** motion data of the Solar System bodies (`Kernels\orbiter.bsp` and others), Orbiter
  does not start without them.
- **Where from:** the links in Orbiter's `COMPILE.md` (`cdn.openorbiter.space/spice/`), NASA/JPL data.
- **Terms:** not stated on the Orbiter site; NASA/JPL data is usually free
  ([TEXTURES.md](TEXTURES.md), section 3). Put into the package by the owner's decision (decision 50).
- **In the repository:** no. **In the test package:** yes — `olc-2-kernels.zip`.
