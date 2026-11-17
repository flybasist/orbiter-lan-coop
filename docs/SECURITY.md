**English** · [Русский](ru/SECURITY.md)

# Security

OLC is meant for computers in one classroom on one local network. Security here is first of
all a clear understanding of what the protocol does not do.

## What is protected

- The plugin accepts connections only when the user has explicitly turned on the host role.
- A client controls only the vessel the host assigned to it (planned).
- Network packets are checked for size and format before parsing: a broken or foreign packet
  is dropped and does not crash Orbiter (tests with broken data are in `tests/`).

## What is NOT protected

- No authentication: any computer on the same network can connect to the host.
- No encryption: data travels over the network in plain form.
- No protection against packet spoofing or deliberate interference inside the network.

## Recommendations

- Do not open the OLC port to the internet and do not forward it on the router.
- Use OLC on a trusted classroom or lab network.
- In Windows Firewall allow the OLC port for the private network only.
