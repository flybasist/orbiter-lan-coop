**English** · [Русский](ru/TEXTURES.md)

# Textures and add-ons for the test package

The research of task 3.15 (decision 43): where planet textures and add-ons come from, how big
they are, what license they carry, and what may go into the archive for the testers. The data was
collected on 2026-11-18 from open pages; the Orbiter forum pages return 403 to the agent, so
**the licenses of the texture packs are not confirmed** (see section 3).

## 1. What the tests need

The tests (task 3.7) are the approach and docking of two DeltaGlider in orbit. They need:

- Orbiter of the base commit with D3D9Client and the OLC plugin - built by us, needs no textures;
- SPICE kernels (`Kernels`, `orbiter.bsp` 1.6 GB) - Orbiter does not start without them;
- base planet textures (Earth first of all): the Orbiter repository has none, and without them
  D3D9Client shows a "Surface textures are missing" window at start (`QUICKSTART.md`, section 4);
- high-resolution surface is not required for orbital docking: the high-resolution packs only
  show close to the surface, at altitude Orbiter uses the coarse levels.

## 2. Where everything comes from

| What | Size | Source | Note |
|---|---|---|---|
| Orbiter 2024 (release) | 373 MB | GitHub `orbitersim/orbiter`, tag `2024`, `Orbiter-x64.zip` | No Earth textures |
| Development build | 1.75 GB | GitHub, tag `latest`, `Orbiter-x64.zip` | Texture content not checked |
| SPICE kernels and `Textures.7z` | 1.2 GB (`Kernels.7z`) | `cdn.openorbiter.space/spice/` | Links from Orbiter `COMPILE.md` |
| Base planet textures | not checked | an Orbiter 2016 installation (Orbiter forum) | As Orbiter `COMPILE.md` says |
| High-res Earth: surface | 15.44 GB | `orbiter-mods.com/files/earth-hires-2016-surf.zip` | Not needed in orbit |
| Earth: elevation | 17.23 GB | `orbiter-mods.com/files/earth-hires-2016-elev.zip` | Not needed in orbit |
| Earth: mask | 2.24 GB | `orbiter-mods.com/files/earth-hires-2016-mask.zip` | Not needed in orbit |
| Earth: clouds | 308 MB | `orbiter-mods.com/files/earth-hires-2016-cloud.zip` | Possible, small |
| Whole Earth | 32.83 GB | torrent on `orbiter-mods.com` | Too big |
| Moon: surface and elevation | 3.74 and 11.78 GB | `orbiter-mods.com/files/moon-hires-2016-*.zip` | Not needed |
| Mars: surface and elevation | 4.04 and 2.63 GB | `orbiter-mods.com/files/mars-hires-2016-*.zip` | Not needed |
| Mercury, Titan, minor bodies | 4 GB, 167 MB, 273 MB | `orbiter-mods.com/files/` | Not needed |

Installing the high-resolution packs: the `.tree` files go to `Textures\<planet>\Archive` of the
Orbiter directory, layers are installed independently (keep the original file).

## 3. Licenses

- Orbiter itself is MIT (the repository `LICENSE` file), D3D9Client is LGPL. They may go into the
  archive with the license texts kept.
- The `orbiter-mods.com` downloads page names MIT only for the Orbiter core; **it states no
  license for the texture packs**. The agent could not read the pack pages on the Orbiter forum (403).
- The source data (NASA and other satellite imagery and elevation) is usually free, but the packs are
  the work of the add-on authors, and the terms of re-publishing differ for each.
- Conclusion: until the owner confirms a pack's license from the author's page, it must not go
  into our archive; it is safe to give the testers **a link and instructions**, not a copy.

## 4. Proposed package contents

1. Into the archive: Orbiter of the base commit with D3D9Client (MIT, LGPL), the OLC plugin,
   settings, scenarios, instructions.
2. SPICE kernels - downloaded from `cdn.openorbiter.space`; they may go into the archive if the owner
   approves (the NASA/JPL kernel license is free, but the Orbiter site does not state it).
3. Base planet textures - from an Orbiter 2016 installation via the forum link; into the archive only
   after the license is confirmed.
4. Do not include high resolution at all: orbital docking does not need it, and the whole Earth is
   33 GB.
5. While there is no permission for textures, an empty `Textures\Earth` directory is enough for the
   testers (Earth without texture, `QUICKSTART.md`, section 4).

The decision (Q10, decision 50): option B — the SPICE kernels in the archive, Earth textures by a link to Orbiter 2016.
