#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only

set -euo pipefail
board="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
root="$(cd -- "$board/../.." && pwd)"
# Reuse the maintained CMake source/include graph. The real STM32 build checks
# board-only drivers, flash registers, the main entry point, and local startup.
build_dir="$(mktemp -d /tmp/robot-hal-tests.XXXXXX)"
cmake -S "$root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DROBOT_SANITIZERS=ON
cmake --build "$build_dir" --parallel 4 --target robot_localization_task_syntax robot_board_hooks_syntax robot_arduino_hal_tests robot_drv8833_unconfigured_tests robot_drv8833_configured_tests
ctest --test-dir "$build_dir" --output-on-failure -R 'robot_(arduino_hal|drv8833_)'
