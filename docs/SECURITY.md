**English** · [Русский](ru/SECURITY.md)

# Security

OLC is meant for computers in one classroom on one local network. Security here is first of
all a clear understanding of what the protocol does not do.

## What is protected

- The plugin accepts connections only when the user has explicitly turned on the host role.
- A client controls only its own vessel: the host accepts `INPUT`, `KEY` and `NAV_STATE` only from its
  owner (`SessionTable`).
- The host carries out pause, time acceleration and save only on the flight director's request.
- Network packets are checked for size and format before parsing: a broken or foreign packet
  is dropped and does not crash Orbiter (tests with broken data are in `tests/`).

## What is NOT protected

- No authentication: any computer on the same network can connect to the host. The host recognises
  the flight director only by the player name (`Director`): anyone can use that name.
- No encryption: data travels over the network in plain form.
- No protection against packet spoofing or deliberate interference inside the network.

## Recommendations

- Do not open the OLC port to the internet and do not forward it on the router.
- Use OLC on a trusted classroom or lab network.
- In Windows Firewall allow the OLC port for the private network only.
