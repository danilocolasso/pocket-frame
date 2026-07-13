#!/bin/sh
# Interactive setup for Pocket Frame. Run via `make setup`.
set -e
cd "$(dirname "$0")"

B='\033[1m'; DIM='\033[2m'; GRN='\033[32m'; CYN='\033[36m'; RED='\033[31m'; YLW='\033[33m'; RST='\033[0m'

printf "\n  ${B}POCKET FRAME${RST} ${DIM}setup${RST}\n"
printf "  ${DIM}────────────────────────────────────────${RST}\n\n"

default_url="http://192.168.1.10:8000/image.png"
while :; do
    printf "  ${CYN}Image URL${RST} ${DIM}[%s]${RST}\n  > " "$default_url"
    read -r url
    url=${url:-$default_url}
    case "$url" in
        http://*|https://*) break ;;
        *) printf "  ${RED}must start with http:// or https://${RST}\n\n" ;;
    esac
done

while :; do
    printf "\n  ${CYN}Refresh interval in seconds${RST} ${DIM}[60, min 5]${RST}\n  > "
    read -r interval
    interval=${interval:-60}
    case "$interval" in
        ''|*[!0-9]*) printf "  ${RED}numbers only${RST}\n" ;;
        *) if [ "$interval" -ge 5 ]; then break; else printf "  ${RED}minimum is 5${RST}\n"; fi ;;
    esac
done

cat > App/PocketFrame/settings.conf <<EOF
url=$url
interval=$interval
selected=0
EOF
printf "\n  ${GRN}✓${RST} App/PocketFrame/settings.conf written\n"

if [ -f App/PocketFrame/pocket-frame ]; then
    printf "  ${GRN}✓${RST} binary already built\n"
elif command -v docker >/dev/null 2>&1; then
    printf "\n  ${CYN}Build the binary now?${RST} ${DIM}(docker, ~1 min) [Y/n]${RST} "
    read -r yn
    case "$yn" in
        n|N) printf "  ${DIM}skipped — run the docker command in docs/manual-setup.md later${RST}\n" ;;
        *)
            docker run --rm --platform linux/amd64 -v "$PWD":/root/workspace \
                aemiii91/miyoomini-toolchain:latest bash -c '. /root/setup-env.sh && make'
            printf "  ${GRN}✓${RST} built App/PocketFrame/pocket-frame\n"
            ;;
    esac
else
    printf "  ${YLW}!${RST} docker not found — see docs/manual-setup.md to build the binary\n"
fi

printf "\n  ${CYN}Device IP to install over SSH${RST} ${DIM}(Onion: Apps > Tweaks > Network > SSH; empty = skip)${RST}\n  > "
read -r ip
if [ -n "$ip" ]; then
    [ -f App/PocketFrame/pocket-frame ] || printf "  ${YLW}!${RST} binary not built yet — installing anyway\n"
    printf "  ${DIM}password is 'onion' by default${RST}\n"
    scp -r App/PocketFrame "onion@$ip:/mnt/SDCARD/App/"
    printf "\n  ${GRN}✓${RST} installed — open ${B}Apps > Pocket Frame${RST} on the device (Wi-Fi on)\n"
else
    printf "  ${DIM}skipped — copy App/PocketFrame/ to /App/ on the SD card${RST}\n"
fi

printf "\n  ${GRN}${B}done${RST}\n\n"
