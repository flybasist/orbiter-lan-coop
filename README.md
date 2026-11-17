**English** · [Русский](README.ru.md)

# orbiter-lan-coop (OLC)

Shared flights in the [Orbiter](https://github.com/orbitersim/orbiter) space flight simulator
over a local network: several people in one simulation from different computers.

> Status: prototype (stage 1). The host sends the world state and the client shows it:
> checked with two Orbiter copies with graphics on one machine, using a recorded DeltaGlider flight.
> Next: a check over the network between two machines.

## How it works

OLC is a plugin for the standard Orbiter, not a modified version of the simulator.

| Role | What it does |
|---|---|
| Host (the instructor's computer) | Computes the physics of all vessels, controls time and pause |
| Client (a student's computer) | Shows the host's world, sends the host commands for its own vessel |

There is one physics for everyone, so the participants' worlds do not drift apart, and docking
the vessels of two players works the same way as in a single-player game.

## Requirements

- Windows 10/11 x64 on all computers (Linux via Wine/Proton, as a client).
- The same Orbiter build and the same set of add-ons on all computers.
- The computers are on one local network.

## Installation and running

There is nothing to install yet. How to set up a developer environment is in
[docs/QUICKSTART.md](docs/QUICKSTART.md), the work plan is in [docs/PLAN.md](docs/PLAN.md).

## Important

- Local network only. The protocol is not secured and is not meant for the internet.
- An unofficial project, not affiliated with the Orbiter team.

## Documentation

All details are in [docs/README.md](docs/README.md). The change history is in [CHANGELOG.md](CHANGELOG.md).

## Contact

Author: Ognev Aleksandr (FlyBasist) — [github.com/flybasist](https://github.com/flybasist)

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE). Third-party components
and their terms are listed in [docs/CREDITS.md](docs/CREDITS.md).
