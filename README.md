**English** · [Русский](README.ru.md)

# orbiter-lan-coop (OLC)

Shared flights in the [Orbiter](https://github.com/orbitersim/orbiter) space flight simulator
over a local network: several people in one simulation from different computers.

> Status: the first version for tests by people is being prepared. A pilot connects with a button in
> Launchpad, gets the host's world and flies their own vessel; everyone sees the dockings and vessel
> mechanisms; the flight director sets a shared pause and time acceleration, saves the world and records
> the flight. Checked on test rigs over the network between machines; docking of two pilots and a lesson
> are yet to be checked by people.

## How it works

OLC is a plugin for the standard Orbiter, not a modified version of the simulator.

| Role | What it does |
|---|---|
| Host (a server, with or without graphics) | Computes the physics of all vessels, controls time and pause |
| Client (a pilot's computer) | Shows the host's world, sends the host commands for its own vessel |
| Flight director (a client without a vessel) | Sees everyone, sets a shared pause and time acceleration, saves the world |

There is one physics for everyone, so the participants' worlds do not drift apart, and docking
the vessels of two players works the same way as in a single-player game.

## Requirements

- Windows 10/11 x64 on all computers (Linux via Wine or Proton is not checked).
- The same Orbiter build and the same set of add-ons on all computers.
- The computers are on one local network.

## Installation and running

Ready archives for testers and their installation — [docs/PACKAGE.md](docs/PACKAGE.md). Building from
source — [docs/QUICKSTART.md](docs/QUICKSTART.md), the work plan — [docs/PLAN.md](docs/PLAN.md).

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
