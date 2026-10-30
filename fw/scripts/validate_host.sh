#!/usr/bin/env bash
set -euo pipefail
# May be invoked from any working directory. Keep generated files out of src/.
fw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake_bin="${CMAKE:-cmake}"
ctest_bin="${CTEST:-ctest}"
build_dir="${1:-${fw_root}/build/validation}"
sanitizers="${ROBOT_SANITIZERS:-OFF}"
"${cmake_bin}" -S "${fw_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Debug -DROBOT_SANITIZERS="${sanitizers}"
build_flags=()
# Force a fresh build after restoring cached workspaces or changing shared ABI.
if [[ "${ROBOT_CLEAN:-OFF}" == "ON" ]]; then build_flags+=(--clean-first); fi
"${cmake_bin}" --build "${build_dir}" "${build_flags[@]}" --parallel "${BUILD_JOBS:-4}"
"${ctest_bin}" --test-dir "${build_dir}" --output-on-failure
