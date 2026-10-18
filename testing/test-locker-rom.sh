#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 4 ]]; then
  echo "Usage: bash $0 OLD_ROM NEW_ROM OLD_SYM NEW_SYM" >&2
  exit 2
fi
old_rom=$(realpath "$1")
new_rom=$(realpath "$2")
old_sym=$(realpath "$3")
new_sym=$(realpath "$4")
root=$(cd "$(dirname "$0")/.." && pwd)

symbol() {
  local result
  result=$(awk -v symbol="$2" '$2 == symbol { print $1; exit }' "$1")
  if [[ -z "$result" ]]; then
    echo "Missing symbol $2 in $1" >&2
    exit 2
  fi
  printf '%s' "$result"
}

old_compare=$(symbol "$old_sym" DumbCheckPartyPassword.compare_strings)
new_compare=$(symbol "$new_sym" DumbCheckPartyPassword.compare_strings)
old_party=$(symbol "$old_sym" DumbCheckPartyPassword)
new_party=$(symbol "$new_sym" DumbCheckPartyPassword)
new_flag=$(symbol "$new_sym" wLockerSolved)
[[ ${old_compare%:*} == "${old_party%:*}" ]]
[[ ${new_compare%:*} == "${new_party%:*}" ]]
[[ $new_flag == 00:cfff ]]

temporary=$(mktemp -d)
trap 'rm -f "$temporary/test"; rmdir "$temporary"' EXIT
cd "$root"
cc -O2 -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -I src \
  testing/locker-rom-test.c \
  src/core.c src/cpu.c src/apu.c src/bit_utils.c src/cheats.c src/colour.c \
  src/debug.c src/hdma.c src/mbc.c src/memory.c src/ops.c src/palettes.c \
  src/ppu.c src/printer.c src/printer_platform/terminal.c src/time_diff.c src/audio_platform/null.c \
  $(pkg-config --cflags --libs libpng epoxy) -lm -pthread -o "$temporary/test"

"$temporary/test" "$old_rom" "$new_rom" \
  "${old_compare#*:}" "${new_compare#*:}" \
  "${old_party#*:}" "${new_party#*:}" \
  dcc7 "${new_flag#*:}" "${old_compare%:*}" "${new_compare%:*}"
