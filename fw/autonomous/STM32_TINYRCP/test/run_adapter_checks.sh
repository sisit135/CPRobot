#!/usr/bin/env bash
set -euo pipefail
board="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
root="$(cd -- "$board/../.." && pwd)"
args=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -I"$board/test/fakes" -I"$board/include" -I"$root/robot_sf/include")
mapfile -t sources < <(find "$board/src" "$root/robot_sf/src" -name '*.cpp' -type f)
"${CXX:-g++}" "${args[@]}" -fsyntax-only "${sources[@]}"
test_bin="$(mktemp /tmp/robot-hal-tests.XXXXXX)"
trap 'rm -f "$test_bin"' EXIT
"${CXX:-g++}" "${args[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer \
 "$board/test/test_arduino_hal.cpp" "$board/src/hal/hal_sensors.cpp" \
 "$root/robot_sf/src/localization/color_tracker.cpp" "$board/src/hal/hal_platform.cpp" -o "$test_bin"
"$test_bin"
for wiring in default example; do
 flags=()
 if [[ "$wiring" == example ]]; then flags=(-DROBOT_EXAMPLE_MOTOR_PINS); fi
 "${CXX:-g++}" "${args[@]}" "${flags[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$board/test/test_drv8833_hal.cpp" "$board/src/hal/hal_motor.cpp" -o "$test_bin"
 "$test_bin"
done
