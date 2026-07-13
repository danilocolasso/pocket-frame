# Pocket Frame

A picture frame app for [Onion OS](https://github.com/OnionUI/Onion) on the
Miyoo Mini+: fetches a remote image over HTTP and shows it fullscreen,
auto-refreshed. Status display, dashboard viewer, or an actual picture frame.

No browser fits in 128MB of RAM — so render anything on a server as a
640×480 PNG (e.g. [commit-frame](https://github.com/danilocolasso/commit-frame))
and let the device do the one thing it does cheaply: download and show it.

## Setup

```sh
make setup
```

The wizard asks for the image URL and refresh interval, builds the binary
(Docker) and installs to the device over SSH. Prefer doing it by hand?
See [docs/manual-setup.md](docs/manual-setup.md).

## Controls

| Button | Welcome | Running |
|---|---|---|
| A | start | refresh now |
| B | exit | back to welcome |
| ↑↓ | select option | — |
| ←→ | change value (hold to accelerate) | switch source |
| X | edit source URL (on-screen keyboard) | — |
| MENU | exit | exit |

Sources, interval and URLs are all editable on-device in the welcome menu —
changes persist to `App/PocketFrame/settings.conf`.
