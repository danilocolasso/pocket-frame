# Pocket Frame

A picture frame app for [Onion OS](https://github.com/OnionUI/Onion) on the
Miyoo Mini+. It fetches a remote image over HTTP and displays it fullscreen,
refreshing on a configurable interval — turn your handheld into a tiny
status display, dashboard viewer, or an actual picture frame.

Everything is configurable on the device itself: sources, refresh interval,
even the URLs (via a d-pad on-screen keyboard). No redeploy needed.

## Why

The Miyoo Mini+ has no browser and 128MB of RAM, so "showing a web page" is
off the table. Pocket Frame flips the problem: render whatever you want on a
server (e.g. headless Chromium screenshotting an HTML dashboard) and let the
device do the one thing it can do cheaply — download and blit a PNG.

```
anything → your server → 640x480 PNG over HTTP → Pocket Frame
```

## Install

1. Grab the `App/PocketFrame/` folder (build the binary first, see below).
2. Copy it to `/App/` on your SD card — or over the network with Onion's
   SSH enabled: `scp -r App/PocketFrame onion@<device-ip>:/mnt/SDCARD/App/`
   (password `onion`).
3. Launch it from Apps → Pocket Frame. Wi-Fi must be on.

## Configure

`App/PocketFrame/settings.conf`:

```
url=http://192.168.1.10:8000/image.png
url=http://192.168.1.10:8000/another.png
interval=60
selected=0
```

- `url=` — one line per source (up to 8). PNG and JPEG are supported;
  anything that fails to decode is discarded.
- `interval=` — refresh period in seconds (min 5, no upper limit).
- `selected=` — active source index.

All of it is also editable in the app's welcome menu, which saves back to
this file on every change.

## Controls

| Button | Welcome | Running |
|---|---|---|
| A | start | refresh now |
| B | exit | back to welcome |
| D-pad ↑↓ | select option | — |
| D-pad ←→ | change value (hold to accelerate) | switch source |
| X | edit source URL | — |
| MENU | exit | exit |

URL editor: d-pad navigates the keyboard, **A** types, **B** deletes,
**START** saves, **MENU** cancels.

## Behavior

- While fetching, a subtle gray dot blinks in the top-right corner; if the
  fetch fails it turns solid red until the next successful refresh, and the
  last good image stays on screen.
- The device won't sleep or auto-shutdown while the app is open
  (`/tmp/stay_awake`, honored by Onion's keymon).
- Images other than 640×480 are centered, not scaled — serve at 640×480.
- Fonts come from Onion itself (`/mnt/SDCARD/miyoo/app/`), curl from
  Onion's `.tmp_update/bin` — nothing is bundled with the app.
- Logs land in `App/PocketFrame/pocket-frame.log`.

## Build

Requires Docker. Uses the official Onion toolchain image:

```sh
docker run --rm --platform linux/amd64 -v "$PWD":/root/workspace \
  aemiii91/miyoomini-toolchain:latest bash -c '. /root/setup-env.sh && make'
```

Produces `App/PocketFrame/pocket-frame` (32-bit ARM, linked only against
SDL 1.2, SDL_image and SDL_ttf — all shipped with Onion).

## Server-side example

Any HTTP server that serves a PNG works. A dashboard pipeline with headless
Chromium, refreshed by cron every minute:

```sh
* * * * * chromium --headless --screenshot=/var/www/image.png \
    --window-size=640,480 --hide-scrollbars http://localhost:3000/dashboard
```
