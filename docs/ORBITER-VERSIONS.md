**English** · [Русский](ru/ORBITER-VERSIONS.md)

# Orbiter 2016, 2024 and the OLC build

For those who flew Orbiter 2016 and will now test OLC: what happened to Orbiter itself over these years,
how our build differs from the familiar 2016 and what OLC is. Installation — [PACKAGE.md](PACKAGE.md),
tests — [TEST-PROCEDURE.md](TEST-PROCEDURE.md).

## 1. Orbiter in short

Orbiter is a free space flight simulator with realistic physics. The author is Martin Schweiger of
University College London; Orbiter has long been used to teach the physics of flight.

| Release | When | What matters |
|---|---|---|
| First version | November 2000 | A free simulator with Newtonian physics |
| 2006, 2010 | 2006, 2010 | Big releases by the author; 2006 brought the scenario editor |
| **2016** | August 2016 | Terrain of the Earth, the Moon and Mars, new planet textures. The last release by the author alone; closed source, 32 bits only |
| Source opened | July 2021 | The author published the sources on GitHub under the free MIT license so that others could develop the project |
| **2024** | 31 December 2024 | The first release built by the community: 64 bits, the D3D9 graphics client and XRSound sound included |
| Development build | constantly | The newest code from GitHub, between releases. Our build is made on it |

The developers do not announce the date of the next release in advance.

## 2. How 2024 and our build differ from 2016

| | Orbiter 2016 | Orbiter 2024 | The OLC build (round 1) |
|---|---|---|---|
| Bits | 32 bits | 64 bits (32 also available) | 64 bits |
| Graphics | Built-in DirectX 7; D3D9Client was installed separately | D3D9Client included | D3D9Client included and already on |
| Earth, Moon, Mars textures | Included | No, taken from 2016 | No, taken from 2016 ([PACKAGE.md](PACKAGE.md), section 2, step 5) |
| Planet ephemerides | Built in | Built in | SPICE kernels (the `olc-2-kernels.zip` archive, 1.3 GB): it does not start without them |
| Stock vessels | DeltaGlider, Shuttle-A, Atlantis, ISS and others | The same | The same |
| Add-ons | A huge choice over many years | Only those rebuilt for the new version | Only those rebuilt for the new version; not needed for the tests |
| Code | Closed | Open (MIT) | Open (MIT), plus the OLC plugin (GPLv3) |

What this means in practice:

- **In look and controls** it is the same Orbiter: Launchpad, scenarios, MFDs, keys, DeltaGlider.
- **Add-ons from 2016 most likely will not work in our build:** 32-bit modules do not load in a 64-bit
  Orbiter. The tests do not need add-ons — only stock vessels (DeltaGlider, Shuttle-A) and the ISS.
- **There is no need to remove Orbiter 2016.** Our build goes into a separate folder (for example `C:\OLC`)
  and does not touch 2016. The planet textures can even be taken from the 2016 installation with one
  settings line (PACKAGE.md).
- **Why the development build and not the 2024 release:** its code is almost two years newer than 2024, and
  OLC follows the main project. OLC is built and checked only on it; whether it works in the 2024 release
  has not been checked. When the next Orbiter release comes out, OLC will move to it.

## 3. What OLC is

OLC (Orbiter LAN Co-op) is a plugin that lets several people fly in one shared Orbiter world from different
computers on one local network. It goes into an ordinary Orbiter; Orbiter itself is not changed.

- **One computes, the others see.** One computer — the host — computes the physics of all vessels. The others
  get the world state from it and send it their pilot's commands. So everyone has the same world, and docking
  two vessels works just as in a single-player game.
- **Everyone flies their own vessel:** a free vessel is taken with the focus (F3); someone else's vessel is
  view only.
- **Everyone sees the mechanisms:** the gear, the nose cone, the navigation frequencies of the pilot's vessel.
- **The flight director** sets a shared pause and time acceleration, saves the world and records the flight
  for a debrief.
- **Local network only** — a classroom, cable or Wi-Fi. It does not work over the internet and is not meant to.

There were attempts at an Orbiter multiplayer before: the best known is OMP (2006–2017), but it was never
finished, and in it everyone computed the physics of their own vessel. In OLC there is one physics for everyone.

## 4. The history of OLC

| When | What |
|---|---|
| 5 November 2026 | The start: studying the Orbiter sources and past multiplayer attempts |
| 7–12 November | The design, the protocol, the network core, the first plugin |
| 17 November | The first checks on computers: the host's world on a client, connecting with one button |
| 18 November | Every pilot's own vessel, docking, a server without graphics |
| 19 November | The flight director, vessel mechanisms for everyone, a check of the code and documents, the test package — **version 0.1** |

Everything is checked on test computers (virtual machines with Windows over a network). What only people can
check is not checked: docking by hand, smoothness by eye, a real classroom network, different graphics cards.
That is why the tests in the club matter so much.

## 5. What is next

- **Tests by the procedure** — the main next step; the form and the logs go to the developer after the lesson.
- **Fixes from the test results** — in the next version.
- **Later:** two people in one vessel (pilot and navigator) and whatever else the testers ask for.
