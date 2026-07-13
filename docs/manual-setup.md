# Manual setup

Everything `make setup` does, by hand.

## 1. Build

Requires Docker (official Onion toolchain image):

```sh
docker run --rm --platform linux/amd64 -v "$PWD":/root/workspace \
  aemiii91/miyoomini-toolchain:latest bash -c '. /root/setup-env.sh && make'
```

Produces `App/PocketFrame/pocket-frame` — 32-bit ARM, linked only against
SDL 1.2, SDL_image and SDL_ttf, all shipped with Onion. Nothing is bundled:
fonts come from `/mnt/SDCARD/miyoo/app/`, curl from Onion's `.tmp_update/bin`.

## 2. Configure

Edit `App/PocketFrame/settings.conf`:

```
url=http://192.168.1.10:8000/image.png
url=http://192.168.1.10:8000/another.png
interval=60
selected=0
```

| Key | Meaning |
|---|---|
| `url=` | one line per source, up to 8 — PNG/JPEG (anything that fails to decode is discarded) |
| `interval=` | refresh period in seconds — min 5, no upper limit |
| `selected=` | active source index |

All of it is also editable later in the app's welcome menu (including the
URLs, via an on-screen keyboard) — changes are saved back to this file.

## 3. Install

Copy `App/PocketFrame/` to `/App/` on the SD card, or over the network with
Onion's SSH enabled (Apps → Tweaks → Network → SSH):

```sh
scp -r App/PocketFrame onion@<device-ip>:/mnt/SDCARD/App/
```

Password: `onion`. Launch from Apps → Pocket Frame, with Wi-Fi on.

## Behavior notes

- While fetching, a subtle gray dot blinks in the top-right corner; if the
  fetch fails it turns solid red until the next successful refresh, and the
  last good image stays on screen.
- The device won't sleep or auto-shutdown while the app is open
  (`/tmp/stay_awake`, honored by Onion's keymon).
- Images other than 640×480 are centered, not scaled — serve at 640×480.
- Logs land in `App/PocketFrame/pocket-frame.log`.

## Serving something to display

Any HTTP server that serves a PNG works. A dashboard pipeline with headless
Chromium, refreshed by cron every minute:

```sh
* * * * * chromium --headless --screenshot=/var/www/image.png \
    --window-size=640,480 --hide-scrollbars http://localhost:3000/dashboard
```

For a ready-made GitHub contribution dashboard, see
[commit-frame](https://github.com/danilocolasso/commit-frame).
