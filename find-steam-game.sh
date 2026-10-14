#!/usr/bin/env bash
#
# Prints the path to "<steam library>/steamapps/common/<game name>" for the
# first Steam library that already has it installed, searching every Steam
# library on the machine (not just the default one). If no library has the
# game yet, prints "<some steam root>/steamapps/common/<game name>" using
# whichever Steam install was found, so the caller can create it there.
# Prints nothing if no Steam install can be found at all.
#
# Usage: find-steam-game.sh "<game name>"
# Override: set STEAM_ROOT to force a specific Steam install to search first.
set -u

GAME_NAME="${1:?usage: find-steam-game.sh <game-name>}"

to_unix_path() {
    # Backslashes -> forward slashes, drop a trailing slash.
    printf '%s' "$1" | sed -e 's#\\\\#/#g' -e 's#\\#/#g' -e 's#/$##'
}

registry_steam_roots() {
    command -v reg.exe >/dev/null 2>&1 || return 0

    for entry in \
        "HKCU\\Software\\Valve\\Steam;SteamPath" \
        "HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam;InstallPath" \
        "HKLM\\SOFTWARE\\Valve\\Steam;InstallPath"
    do
        key="${entry%;*}"
        value="${entry#*;}"
        # MSYS2_ARG_CONV_EXCL stops MSYS2 from mangling the backslashes in
        # the registry key path before reg.exe ever sees them.
        raw=$(MSYS2_ARG_CONV_EXCL="*" reg.exe query "$key" /v "$value" 2>/dev/null \
            | grep -i "$value" | sed -E 's/.*REG_SZ[[:space:]]+//')
        [ -n "$raw" ] && to_unix_path "$raw"
    done
}

candidate_roots() {
    [ -n "${STEAM_ROOT:-}" ] && printf '%s\n' "$STEAM_ROOT"
    registry_steam_roots
    [ -n "${HOME:-}" ] && printf '%s\n' \
        "$HOME/.local/share/Steam" \
        "$HOME/.steam/steam" \
        "$HOME/.steam/root" \
        "$HOME/Library/Application Support/Steam" \
        "$HOME/Steam"
    printf '%s\n' \
        "/c/Program Files (x86)/Steam" \
        "/mnt/c/Program Files (x86)/Steam"
}

# Every library Steam knows about for a given root: the root itself plus
# every "path" entry in its libraryfolders.vdf (additional drives/folders).
library_paths_for_root() {
    root="$1"
    printf '%s\n' "$root"

    for vdf in "$root/steamapps/libraryfolders.vdf" "$root/config/libraryfolders.vdf"; do
        [ -f "$vdf" ] || continue
        grep -E '^[[:space:]]*"path"[[:space:]]*"' "$vdf" \
            | sed -E 's/^[^"]*"path"[[:space:]]*"([^"]*)".*/\1/' \
            | while IFS= read -r p; do to_unix_path "$p"; done
    done
}

roots_file=$(mktemp)
libs_file=$(mktemp)
trap 'rm -f "$roots_file" "$libs_file"' EXIT

candidate_roots > "$roots_file"

best_guess=""
found=""
seen_roots=""

while IFS= read -r root; do
    [ -n "$root" ] && [ -d "$root" ] || continue
    case " $seen_roots " in *" $root "*) continue ;; esac
    seen_roots="$seen_roots $root"

    library_paths_for_root "$root" > "$libs_file"

    while IFS= read -r lib; do
        [ -n "$lib" ] && [ -d "$lib" ] || continue

        candidate="$lib/steamapps/common/$GAME_NAME"
        [ -z "$best_guess" ] && best_guess="$candidate"

        if [ -z "$found" ] && [ -d "$candidate" ]; then
            found="$candidate"
        fi
    done < "$libs_file"
done < "$roots_file"

if [ -n "$found" ]; then
    printf '%s\n' "$found"
elif [ -n "$best_guess" ]; then
    printf '%s\n' "$best_guess"
fi
